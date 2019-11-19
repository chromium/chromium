// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/geolocation/simple_geolocation_request.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/geolocation/simple_geolocation_request_test_monitor.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

// Location resolve timeout is usually 1 minute, so 2 minutes with 50 buckets
// should be enough.
#define UMA_HISTOGRAM_LOCATION_RESPONSE_TIMES(name, sample)         \
  UMA_HISTOGRAM_CUSTOM_TIMES(name,                                  \
                             sample,                                \
                             base::TimeDelta::FromMilliseconds(10), \
                             base::TimeDelta::FromMinutes(2),       \
                             50)

namespace chromeos {

namespace {

// Used if sending location signals (WiFi APs, cell towers, etc) is disabled.
constexpr char kSimpleGeolocationRequestBody[] = "{\"considerIp\": \"true\"}";

// Geolocation request field keys:
// Top-level request data fields.
constexpr char kConsiderIp[] = "considerIp";
constexpr char kWifiAccessPoints[] = "wifiAccessPoints";
constexpr char kCellTowers[] = "cellTowers";
// Shared Wifi and Cell Tower objects.
constexpr char kAge[] = "age";
constexpr char kSignalStrength[] = "signalStrength";
// WiFi access point objects.
constexpr char kMacAddress[] = "macAddress";
constexpr char kChannel[] = "channel";
constexpr char kSignalToNoiseRatio[] = "signalToNoiseRatio";
// Cell tower objects.
constexpr char kCellId[] = "cellId";
constexpr char kLocationAreaCode[] = "locationAreaCode";
constexpr char kMobileCountryCode[] = "mobileCountryCode";
constexpr char kMobileNetworkCode[] = "mobileNetworkCode";

// Geolocation response field keys:
constexpr char kLocationString[] = "location";
constexpr char kLatString[] = "lat";
constexpr char kLngString[] = "lng";
constexpr char kAccuracyString[] = "accuracy";

// Error object and its contents.
constexpr char kErrorString[] = "error";
// "errors" array in "erorr" object is ignored.
constexpr char kCodeString[] = "code";
constexpr char kMessageString[] = "message";

// We are using "sparse" histograms for the number of retry attempts,
// so we need to explicitly limit maximum value (in case something goes wrong).
const size_t kMaxRetriesValueInHistograms = 20;

// Sleep between geolocation request retry on HTTP error.
const unsigned int kResolveGeolocationRetrySleepOnServerErrorSeconds = 5;

// Sleep between geolocation request retry on bad server response.
const unsigned int kResolveGeolocationRetrySleepBadResponseSeconds = 10;

enum SimpleGeolocationRequestEvent {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.
  SIMPLE_GEOLOCATION_REQUEST_EVENT_REQUEST_START = 0,
  SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_SUCCESS = 1,
  SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_NOT_OK = 2,
  SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_EMPTY = 3,
  SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_MALFORMED = 4,

  // NOTE: Add entries only immediately above this line.
  SIMPLE_GEOLOCATION_REQUEST_EVENT_COUNT = 5
};

enum SimpleGeolocationRequestResult {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.
  SIMPLE_GEOLOCATION_REQUEST_RESULT_SUCCESS = 0,
  SIMPLE_GEOLOCATION_REQUEST_RESULT_FAILURE = 1,
  SIMPLE_GEOLOCATION_REQUEST_RESULT_SERVER_ERROR = 2,
  SIMPLE_GEOLOCATION_REQUEST_RESULT_CANCELLED = 3,

  // NOTE: Add entries only immediately above this line.
  SIMPLE_GEOLOCATION_REQUEST_RESULT_COUNT = 4
};

SimpleGeolocationRequestTestMonitor* g_test_request_hook = nullptr;

// Too many requests (more than 1) mean there is a problem in implementation.
void RecordUmaEvent(SimpleGeolocationRequestEvent event) {
  UMA_HISTOGRAM_ENUMERATION("SimpleGeolocation.Request.Event",
                            event,
                            SIMPLE_GEOLOCATION_REQUEST_EVENT_COUNT);
}

void RecordUmaResponseCode(int code) {
  base::UmaHistogramSparse("SimpleGeolocation.Request.ResponseCode", code);
}

// Slow geolocation resolve leads to bad user experience.
void RecordUmaResponseTime(base::TimeDelta elapsed, bool success) {
  if (success) {
    UMA_HISTOGRAM_LOCATION_RESPONSE_TIMES(
        "SimpleGeolocation.Request.ResponseSuccessTime", elapsed);
  } else {
    UMA_HISTOGRAM_LOCATION_RESPONSE_TIMES(
        "SimpleGeolocation.Request.ResponseFailureTime", elapsed);
  }
}

void RecordUmaResult(SimpleGeolocationRequestResult result, size_t retries) {
  UMA_HISTOGRAM_ENUMERATION("SimpleGeolocation.Request.Result",
                            result,
                            SIMPLE_GEOLOCATION_REQUEST_RESULT_COUNT);
  base::UmaHistogramSparse("SimpleGeolocation.Request.Retries",
                           std::min(retries, kMaxRetriesValueInHistograms));
}

// Creates the request url to send to the server.
GURL GeolocationRequestURL(const GURL& url) {
  if (url != SimpleGeolocationProvider::DefaultGeolocationProviderURL())
    return url;

  std::string api_key = google_apis::GetAPIKey();
  if (api_key.empty())
    return url;

  std::string query(url.query());
  if (!query.empty())
    query += "&";
  query += "key=" + net::EscapeQueryParamValue(api_key, true);
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

void PrintGeolocationError(const GURL& server_url,
                           const std::string& message,
                           Geoposition* position) {
  position->status = Geoposition::STATUS_SERVER_ERROR;
  position->error_message =
      base::StringPrintf("SimpleGeolocation provider at '%s' : %s.",
                         server_url.GetOrigin().spec().c_str(),
                         message.c_str());
  VLOG(1) << "SimpleGeolocationRequest::GetGeolocationFromResponse() : "
          << position->error_message;
}

// Parses the server response body. Returns true if parsing was successful.
// Sets |*position| to the parsed Geolocation if a valid position was received,
// otherwise leaves it unchanged.
bool ParseServerResponse(const GURL& server_url,
                         const std::string& response_body,
                         Geoposition* position) {
  DCHECK(position);

  if (response_body.empty()) {
    PrintGeolocationError(
        server_url, "Server returned empty response", position);
    RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_EMPTY);
    return false;
  }
  VLOG(1) << "SimpleGeolocationRequest::ParseServerResponse() : "
             "Parsing response '" << response_body << "'";

  // Parse the response, ignoring comments.
  std::string error_msg;
  std::unique_ptr<base::Value> response_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          response_body, base::JSON_PARSE_RFC, NULL, &error_msg);
  if (response_value == NULL) {
    PrintGeolocationError(
        server_url, "JSONReader failed: " + error_msg, position);
    RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  base::DictionaryValue* response_object = NULL;
  if (!response_value->GetAsDictionary(&response_object)) {
    PrintGeolocationError(
        server_url,
        "Unexpected response type : " +
            base::StringPrintf(
                "%u", static_cast<unsigned int>(response_value->type())),
        position);
    RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  base::DictionaryValue* error_object = NULL;
  base::DictionaryValue* location_object = NULL;
  response_object->GetDictionaryWithoutPathExpansion(kLocationString,
                                                     &location_object);
  response_object->GetDictionaryWithoutPathExpansion(kErrorString,
                                                     &error_object);

  position->timestamp = base::Time::Now();

  if (error_object) {
    if (!error_object->GetStringWithoutPathExpansion(
            kMessageString, &(position->error_message))) {
      position->error_message = "Server returned error without message.";
    }

    // Ignore result (code defaults to zero).
    error_object->GetIntegerWithoutPathExpansion(kCodeString,
                                                 &(position->error_code));
  } else {
    position->error_message.erase();
  }

  if (location_object) {
    if (!location_object->GetDoubleWithoutPathExpansion(
            kLatString, &(position->latitude))) {
      PrintGeolocationError(server_url, "Missing 'lat' attribute.", position);
      RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_MALFORMED);
      return false;
    }
    if (!location_object->GetDoubleWithoutPathExpansion(
            kLngString, &(position->longitude))) {
      PrintGeolocationError(server_url, "Missing 'lon' attribute.", position);
      RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_MALFORMED);
      return false;
    }
    if (!response_object->GetDoubleWithoutPathExpansion(
            kAccuracyString, &(position->accuracy))) {
      PrintGeolocationError(
          server_url, "Missing 'accuracy' attribute.", position);
      RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_MALFORMED);
      return false;
    }
  }

  if (error_object) {
    position->status = Geoposition::STATUS_SERVER_ERROR;
    return false;
  }
  // Empty response is STATUS_OK but not Valid().
  position->status = Geoposition::STATUS_OK;
  return true;
}

// Attempts to extract a position from the response. Detects and indicates
// various failure cases.
bool GetGeolocationFromResponse(bool http_success,
                                int status_code,
                                const std::string& response_body,
                                const GURL& server_url,
                                Geoposition* position) {
  VLOG(1) << "GetGeolocationFromResponse(http_success=" << http_success
          << ", status_code=" << status_code << "): response_body:\n"
          << response_body;

  // HttpPost can fail for a number of reasons. Most likely this is because
  // we're offline, or there was no response.
  if (!http_success) {
    PrintGeolocationError(server_url, "No response received", position);
    RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_EMPTY);
    return false;
  }
  if (status_code != net::HTTP_OK) {
    std::string message = "Returned error code ";
    message += base::NumberToString(status_code);
    PrintGeolocationError(server_url, message, position);
    RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_RESPONSE_NOT_OK);
    return false;
  }

  return ParseServerResponse(server_url, response_body, position);
}

void ReportUmaHasWiFiAccessPoints(bool value) {
  UMA_HISTOGRAM_BOOLEAN("SimpleGeolocation.Request.HasWiFiAccessPoints", value);
}
void ReportUmaHasCellTowers(bool value) {
  UMA_HISTOGRAM_BOOLEAN("SimpleGeolocation.Request.HasCellTowers", value);
}

// Helpers to reformat data into dictionaries for conversion to request JSON
std::unique_ptr<base::DictionaryValue> CreateAccessPointDictionary(
    WifiAccessPoint access_point) {
  auto access_point_dictionary = std::make_unique<base::DictionaryValue>();

  access_point_dictionary->SetKey(kMacAddress,
                                  base::Value(access_point.mac_address));
  access_point_dictionary->SetKey(kSignalStrength,
                                  base::Value(access_point.signal_strength));
  if (!access_point.timestamp.is_null()) {
    access_point_dictionary->SetKey(
        kAge,
        base::Value(base::NumberToString(
            (base::Time::Now() - access_point.timestamp).InMilliseconds())));
  }

  access_point_dictionary->SetKey(kChannel, base::Value(access_point.channel));
  access_point_dictionary->SetKey(kSignalToNoiseRatio,
                                  base::Value(access_point.signal_to_noise));

  return access_point_dictionary;
}

std::unique_ptr<base::DictionaryValue> CreateCellTowerDictionary(
    CellTower cell_tower) {
  auto cell_tower_dictionary = std::make_unique<base::DictionaryValue>();
  cell_tower_dictionary->SetKey(kCellId, base::Value(cell_tower.ci));
  cell_tower_dictionary->SetKey(kLocationAreaCode, base::Value(cell_tower.lac));
  cell_tower_dictionary->SetKey(kMobileCountryCode,
                                base::Value(cell_tower.mcc));
  cell_tower_dictionary->SetKey(kMobileNetworkCode,
                                base::Value(cell_tower.mnc));

  if (!cell_tower.timestamp.is_null()) {
    cell_tower_dictionary->SetKey(
        kAge,
        base::Value(base::NumberToString(
            (base::Time::Now() - cell_tower.timestamp).InMilliseconds())));
  }
  return cell_tower_dictionary;
}

}  // namespace

SimpleGeolocationRequest::SimpleGeolocationRequest(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const GURL& service_url,
    base::TimeDelta timeout,
    std::unique_ptr<WifiAccessPointVector> wifi_data,
    std::unique_ptr<CellTowerVector> cell_tower_data)
    : shared_url_loader_factory_(std::move(factory)),
      service_url_(service_url),
      retry_sleep_on_server_error_(base::TimeDelta::FromSeconds(
          kResolveGeolocationRetrySleepOnServerErrorSeconds)),
      retry_sleep_on_bad_response_(base::TimeDelta::FromSeconds(
          kResolveGeolocationRetrySleepBadResponseSeconds)),
      timeout_(timeout),
      retries_(0),
      wifi_data_(wifi_data.release()),
      cell_tower_data_(cell_tower_data.release()) {}

SimpleGeolocationRequest::~SimpleGeolocationRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If callback is not empty, request is cancelled.
  if (callback_) {
    RecordUmaResponseTime(base::Time::Now() - request_started_at_, false);
    RecordUmaResult(SIMPLE_GEOLOCATION_REQUEST_RESULT_CANCELLED, retries_);
  }

  if (g_test_request_hook)
    g_test_request_hook->OnRequestCreated(this);
}

std::string SimpleGeolocationRequest::FormatRequestBody() const {
  if (!wifi_data_)
    ReportUmaHasWiFiAccessPoints(false);

  if (!cell_tower_data_)
    ReportUmaHasCellTowers(false);

  if (!cell_tower_data_ && !wifi_data_)
    return std::string(kSimpleGeolocationRequestBody);

  std::unique_ptr<base::DictionaryValue> request(new base::DictionaryValue);
  request->SetKey(kConsiderIp, base::Value(true));

  if (wifi_data_) {
    auto wifi_access_points = std::make_unique<base::ListValue>();
    for (const WifiAccessPoint& access_point : *wifi_data_) {
      wifi_access_points->Append(CreateAccessPointDictionary(access_point));
    }
    request->SetWithoutPathExpansion(kWifiAccessPoints,
                                     std::move(wifi_access_points));
  }

  if (cell_tower_data_) {
    auto cell_towers = std::make_unique<base::ListValue>();
    for (const CellTower& cell_tower : *cell_tower_data_) {
      cell_towers->Append(CreateCellTowerDictionary(cell_tower));
    }
    request->SetWithoutPathExpansion(kCellTowers, std::move(cell_towers));
  }

  std::string result;
  if (!base::JSONWriter::Write(*request, &result)) {
    // If there's no data for a network type, we will have already reported
    // false above
    if (wifi_data_)
      ReportUmaHasWiFiAccessPoints(false);
    if (cell_tower_data_)
      ReportUmaHasCellTowers(false);

    return std::string(kSimpleGeolocationRequestBody);
  }

  if (wifi_data_)
    ReportUmaHasWiFiAccessPoints(wifi_data_->size());
  if (cell_tower_data_)
    ReportUmaHasCellTowers(cell_tower_data_->size());

  return result;
}

void SimpleGeolocationRequest::StartRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  RecordUmaEvent(SIMPLE_GEOLOCATION_REQUEST_EVENT_REQUEST_START);
  ++retries_;

  const std::string request_body = FormatRequestBody();
  VLOG(1) << "SimpleGeolocationRequest::StartRequest(): request body:\n"
          << request_body;

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = request_url_;
  request->method = "POST";
  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(request), NO_TRAFFIC_ANNOTATION_YET);
  simple_url_loader_->AttachStringForUpload(request_body, "application/json");

  // Call test hook before asynchronous request actually starts.
  if (g_test_request_hook)
    g_test_request_hook->OnStart(this);

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      shared_url_loader_factory_.get(),
      base::BindOnce(&SimpleGeolocationRequest::OnSimpleURLLoaderComplete,
                     base::Unretained(this)));
}

void SimpleGeolocationRequest::MakeRequest(ResponseCallback callback) {
  callback_ = std::move(callback);
  request_url_ = GeolocationRequestURL(service_url_);
  timeout_timer_.Start(
      FROM_HERE, timeout_, this, &SimpleGeolocationRequest::OnTimeout);
  request_started_at_ = base::Time::Now();
  StartRequest();
}

// static
void SimpleGeolocationRequest::SetTestMonitor(
    SimpleGeolocationRequestTestMonitor* monitor) {
  g_test_request_hook = monitor;
}

std::string SimpleGeolocationRequest::FormatRequestBodyForTesting() const {
  return FormatRequestBody();
}

void SimpleGeolocationRequest::Retry(bool server_error) {
  base::TimeDelta delay(server_error ? retry_sleep_on_server_error_
                                     : retry_sleep_on_bad_response_);
  request_scheduled_.Start(
      FROM_HERE, delay, this, &SimpleGeolocationRequest::StartRequest);
}

void SimpleGeolocationRequest::OnSimpleURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  bool is_success = !!response_body;
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  RecordUmaResponseCode(response_code);

  const bool parse_success = GetGeolocationFromResponse(
      is_success, response_code, response_body ? *response_body : std::string(),
      simple_url_loader_->GetFinalURL(), &position_);
  // Note that SimpleURLLoader doesn't return a body for non-2xx
  // responses by default.
  const bool server_error =
      (!is_success && (response_code == -1 || response_code / 100 == 2)) ||
      (response_code >= 500 && response_code < 600);
  const bool success = parse_success && position_.Valid();
  simple_url_loader_.reset();

  DVLOG(1)
      << "SimpleGeolocationRequest::OnSimpleURLLoaderComplete(): position={"
      << position_.ToString() << "}";

  if (!success) {
    Retry(server_error);
    return;
  }
  const base::TimeDelta elapsed = base::Time::Now() - request_started_at_;
  RecordUmaResponseTime(elapsed, success);

  RecordUmaResult(SIMPLE_GEOLOCATION_REQUEST_RESULT_SUCCESS, retries_);

  ReplyAndDestroySelf(elapsed, server_error);
  // "this" is already destroyed here.
}

void SimpleGeolocationRequest::ReplyAndDestroySelf(
    const base::TimeDelta elapsed,
    bool server_error) {
  simple_url_loader_.reset();
  timeout_timer_.Stop();
  request_scheduled_.Stop();

  ResponseCallback callback = std::move(callback_);

  // Empty callback is used to identify "completed or not yet started request".
  callback_.Reset();

  // callback.Run() usually destroys SimpleGeolocationRequest, because this is
  // the way callback is implemented in GeolocationProvider.
  std::move(callback).Run(position_, server_error, elapsed);
  // "this" is already destroyed here.
}

void SimpleGeolocationRequest::OnTimeout() {
  const SimpleGeolocationRequestResult result =
      (position_.status == Geoposition::STATUS_SERVER_ERROR
           ? SIMPLE_GEOLOCATION_REQUEST_RESULT_SERVER_ERROR
           : SIMPLE_GEOLOCATION_REQUEST_RESULT_FAILURE);
  RecordUmaResult(result, retries_);
  position_.status = Geoposition::STATUS_TIMEOUT;
  const base::TimeDelta elapsed = base::Time::Now() - request_started_at_;
  ReplyAndDestroySelf(elapsed, true /* server_error */);
  // "this" is already destroyed here.
}

}  // namespace chromeos
