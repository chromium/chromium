// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/timezone/timezone_request.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {

namespace {

const char kDefaultTimezoneProviderUrl[] =
    "https://maps.googleapis.com/maps/api/timezone/json?";

const char kKeyString[] = "key";
// Language parameter is unsupported for now.
// const char kLanguageString[] = "language";
const char kLocationString[] = "location";
const char kSensorString[] = "sensor";
const char kTimestampString[] = "timestamp";

const char kDstOffsetString[] = "dstOffset";
const char kRawOffsetString[] = "rawOffset";
const char kTimeZoneIdString[] = "timeZoneId";
const char kTimeZoneNameString[] = "timeZoneName";
const char kStatusString[] = "status";
const char kErrorMessageString[] = "error_message";

// Sleep between timezone request retry on HTTP error.
const unsigned int kResolveTimeZoneRetrySleepOnServerErrorSeconds = 5;

// Sleep between timezone request retry on bad server response.
const unsigned int kResolveTimeZoneRetrySleepBadResponseSeconds = 10;

struct StatusString2Enum {
  const char* string;
  TimeZoneResponseData::Status value;
};

const StatusString2Enum statusString2Enum[] = {
    {"OK", TimeZoneResponseData::OK},
    {"INVALID_REQUEST", TimeZoneResponseData::INVALID_REQUEST},
    {"OVER_QUERY_LIMIT", TimeZoneResponseData::OVER_QUERY_LIMIT},
    {"REQUEST_DENIED", TimeZoneResponseData::REQUEST_DENIED},
    {"UNKNOWN_ERROR", TimeZoneResponseData::UNKNOWN_ERROR},
    {"ZERO_RESULTS", TimeZoneResponseData::ZERO_RESULTS},
};

enum TimeZoneRequestEvent {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.
  TIMEZONE_REQUEST_EVENT_REQUEST_START = 0,
  TIMEZONE_REQUEST_EVENT_RESPONSE_SUCCESS = 1,
  TIMEZONE_REQUEST_EVENT_RESPONSE_NOT_OK = 2,
  TIMEZONE_REQUEST_EVENT_RESPONSE_EMPTY = 3,
  TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED = 4,

  // NOTE: Add entries only immediately above this line.
  TIMEZONE_REQUEST_EVENT_COUNT = 5
};

enum TimeZoneRequestResult {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.
  TIMEZONE_REQUEST_RESULT_SUCCESS = 0,
  TIMEZONE_REQUEST_RESULT_FAILURE = 1,
  TIMEZONE_REQUEST_RESULT_SERVER_ERROR = 2,
  TIMEZONE_REQUEST_RESULT_CANCELLED = 3,

  // NOTE: Add entries only immediately above this line.
  TIMEZONE_REQUEST_RESULT_COUNT = 4
};

constexpr net::NetworkTrafficAnnotationTag kTimezoneRequestNetworkTag =
    net::DefineNetworkTrafficAnnotation("timezone_lookup", R"(
        semantics {
          sender: "Timezone Resolver"
          description:
            "Determine the user's timezone based on their geolocation."
          trigger:
            "Triggered during the device setup wizard, on boot, on user "
            "session start, and in regular intervals."
          data:
            "The user's geolocation. By default, the geolocation is determined "
            "from the ip address of the user, but depending on policies and "
            "preferences, the user's Wi-Fi and mobile networks can also be "
            "taken into account."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable automatic timezone detection in Chrome "
            "OS settings under 'Advanced' -> 'Date and time' -> 'Timezone' by "
            "specifying a fixed timezone under 'Choose from list'. When you do "
            "not specify a fixed timezone, you can select whether just the ip "
            "address or also Wi-Fi and mobile networks are used to determine "
            "your geolocation."
          chrome_device_policy {
            # SystemTimezone
            system_timezone {
              # cmfcmf: HasSystemTimezonePolicy
              timezone: 'not empty, e.g. Europe/Berlin'
            }
          }
          chrome_device_policy {
            # SystemTimezone
            system_timezone {
              timezone_detection_type: DISABLED
            }
          }
          chrome_device_policy {
            # SystemTimezone
            system_timezone {
              timezone_detection_type: IP_ONLY
            }
          }
        })");

// Too many requests (more than 1) mean there is a problem in implementation.
void RecordUmaEvent(TimeZoneRequestEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "TimeZone.TimeZoneRequest.Event", event, TIMEZONE_REQUEST_EVENT_COUNT);
}

void RecordUmaResponseCode(int code) {
  base::UmaHistogramSparse("TimeZone.TimeZoneRequest.ResponseCode", code);
}

// Slow timezone resolve leads to bad user experience.
void RecordUmaResponseTime(base::TimeDelta elapsed, bool success) {
  if (success) {
    UMA_HISTOGRAM_TIMES("TimeZone.TimeZoneRequest.ResponseSuccessTime",
                        elapsed);
  } else {
    UMA_HISTOGRAM_TIMES("TimeZone.TimeZoneRequest.ResponseFailureTime",
                        elapsed);
  }
}

void RecordUmaResult(TimeZoneRequestResult result, unsigned retries) {
  UMA_HISTOGRAM_ENUMERATION(
      "TimeZone.TimeZoneRequest.Result", result, TIMEZONE_REQUEST_RESULT_COUNT);
  base::UmaHistogramSparse("TimeZone.TimeZoneRequest.Retries", retries);
}

// Creates the request url to send to the server.
// |sensor| if this location was determined using hardware sensor.
GURL TimeZoneRequestURL(const GURL& url,
                        const Geoposition& geoposition,
                        bool sensor) {
  std::string query(url.query());
  query += base::StringPrintf(
      "%s=%f,%f", kLocationString, geoposition.latitude, geoposition.longitude);
  if (url == DefaultTimezoneProviderURL()) {
    std::string api_key = google_apis::GetAPIKey();
    if (!api_key.empty()) {
      query += "&";
      query += kKeyString;
      query += "=";
      query += base::EscapeQueryParamValue(api_key, true);
    }
  }
  if (!geoposition.timestamp.is_null()) {
    query += base::StringPrintf(
        "&%s=%ld", kTimestampString, geoposition.timestamp.ToTimeT());
  }
  query += "&";
  query += kSensorString;
  query += "=";
  query += (sensor ? "true" : "false");

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

void PrintTimeZoneError(const GURL& server_url,
                        const std::string& message,
                        TimeZoneResponseData* timezone) {
  timezone->status = TimeZoneResponseData::REQUEST_ERROR;
  timezone->error_message = base::StringPrintf(
      "TimeZone provider at '%s' : %s.",
      server_url.DeprecatedGetOriginAsURL().spec().c_str(), message.c_str());
  LOG(WARNING) << "TimeZoneRequest::GetTimeZoneFromResponse() : "
               << timezone->error_message;
}

// Parses the server response body. Returns true if parsing was successful.
// Sets |*timezone| to the parsed TimeZone if a valid timezone was received,
// otherwise leaves it unchanged.
bool ParseServerResponse(const GURL& server_url,
                         const std::string& response_body,
                         TimeZoneResponseData* timezone) {
  DCHECK(timezone);

  if (response_body.empty()) {
    PrintTimeZoneError(server_url, "Server returned empty response", timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_EMPTY);
    return false;
  }
  VLOG(1) << "TimeZoneRequest::ParseServerResponse() : Parsing response "
          << response_body;

  // Parse the response, ignoring comments.
  auto parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(response_body);
  if (!parsed_json.has_value()) {
    PrintTimeZoneError(server_url,
                       "JSONReader failed: " + parsed_json.error().message,
                       timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  if (!parsed_json->is_dict()) {
    PrintTimeZoneError(server_url,
                       "Unexpected response type : " +
                           base::StringPrintf("%u", static_cast<unsigned int>(
                                                        parsed_json->type())),
                       timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  base::Value::Dict response_object = std::move(*parsed_json).TakeDict();

  const std::string* status = response_object.FindString(kStatusString);

  if (!status) {
    PrintTimeZoneError(server_url, "Missing status attribute.", timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  bool found = false;
  for (size_t i = 0; i < std::size(statusString2Enum); ++i) {
    if (*status != statusString2Enum[i].string)
      continue;

    timezone->status = statusString2Enum[i].value;
    found = true;
    break;
  }

  if (!found) {
    PrintTimeZoneError(
        server_url, "Bad status attribute value: '" + *status + "'", timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  const bool status_ok = (timezone->status == TimeZoneResponseData::OK);

  std::optional<double> dst_offset =
      response_object.FindDouble(kDstOffsetString);
  if (dst_offset.has_value()) {
    timezone->dstOffset = dst_offset.value();
  } else if (status_ok) {
    PrintTimeZoneError(server_url, "Missing dstOffset attribute.", timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  std::optional<double> raw_offset =
      response_object.FindDouble(kRawOffsetString);
  if (raw_offset.has_value()) {
    timezone->rawOffset = raw_offset.value();
  } else if (status_ok) {
    PrintTimeZoneError(server_url, "Missing rawOffset attribute.", timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  const std::string* time_zone_id =
      response_object.FindString(kTimeZoneIdString);
  if (time_zone_id) {
    timezone->timeZoneId = *time_zone_id;
  } else if (status_ok) {
    PrintTimeZoneError(server_url, "Missing timeZoneId attribute.", timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  const std::string* time_zone_name =
      response_object.FindString(kTimeZoneNameString);
  if (time_zone_name) {
    timezone->timeZoneName = *time_zone_name;
  } else if (status_ok) {
    PrintTimeZoneError(server_url, "Missing timeZoneName attribute.", timezone);
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_MALFORMED);
    return false;
  }

  // "error_message" field is optional. Ignore result.
  const std::string* error_message =
      response_object.FindString(kErrorMessageString);
  if (error_message)
    timezone->error_message = *error_message;

  return true;
}

// Attempts to extract a position from the response. Detects and indicates
// various failure cases.
std::unique_ptr<TimeZoneResponseData> GetTimeZoneFromResponse(
    bool http_success,
    int status_code,
    const std::string& response_body,
    const GURL& server_url) {
  std::unique_ptr<TimeZoneResponseData> timezone(new TimeZoneResponseData);

  // HttpPost can fail for a number of reasons. Most likely this is because
  // we're offline, or there was no response.
  if (!http_success) {
    PrintTimeZoneError(server_url, "No response received", timezone.get());
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_EMPTY);
    return timezone;
  }
  if (status_code != net::HTTP_OK) {
    std::string message = "Returned error code ";
    message += base::NumberToString(status_code);
    PrintTimeZoneError(server_url, message, timezone.get());
    RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_NOT_OK);
    return timezone;
  }

  if (!ParseServerResponse(server_url, response_body, timezone.get()))
    return timezone;

  RecordUmaEvent(TIMEZONE_REQUEST_EVENT_RESPONSE_SUCCESS);
  return timezone;
}

}  // namespace

TimeZoneResponseData::TimeZoneResponseData()
    : dstOffset(0), rawOffset(0), status(ZERO_RESULTS) {}

GURL DefaultTimezoneProviderURL() {
  return GURL(kDefaultTimezoneProviderUrl);
}

TimeZoneRequest::TimeZoneRequest(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const GURL& service_url,
    const Geoposition& geoposition,
    base::TimeDelta retry_timeout)
    : shared_url_loader_factory_(std::move(factory)),
      service_url_(service_url),
      geoposition_(geoposition),
      retry_timeout_abs_(base::Time::Now() + retry_timeout),
      retry_sleep_on_server_error_(
          base::Seconds(kResolveTimeZoneRetrySleepOnServerErrorSeconds)),
      retry_sleep_on_bad_response_(
          base::Seconds(kResolveTimeZoneRetrySleepBadResponseSeconds)),
      retries_(0) {}

TimeZoneRequest::~TimeZoneRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If callback is not empty, request is cancelled.
  if (!callback_.is_null()) {
    RecordUmaResponseTime(base::Time::Now() - request_started_at_, false);
    RecordUmaResult(TIMEZONE_REQUEST_RESULT_CANCELLED, retries_);
  }
}

void TimeZoneRequest::StartRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordUmaEvent(TIMEZONE_REQUEST_EVENT_REQUEST_START);
  request_started_at_ = base::Time::Now();
  ++retries_;

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = request_url_;
  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  url_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                 kTimezoneRequestNetworkTag);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      shared_url_loader_factory_.get(),
      base::BindOnce(&TimeZoneRequest::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void TimeZoneRequest::MakeRequest(TimeZoneResponseCallback callback) {
  callback_ = std::move(callback);
  request_url_ =
      TimeZoneRequestURL(service_url_, geoposition_, false /* sensor */);
  StartRequest();
}

void TimeZoneRequest::Retry(bool server_error) {
  const base::TimeDelta delay(server_error ? retry_sleep_on_server_error_
                                           : retry_sleep_on_bad_response_);
  timezone_request_scheduled_.Start(
      FROM_HERE, delay, this, &TimeZoneRequest::StartRequest);
}

void TimeZoneRequest::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  bool is_success = !!response_body;
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  RecordUmaResponseCode(response_code);

  std::string data;
  std::unique_ptr<TimeZoneResponseData> timezone = GetTimeZoneFromResponse(
      is_success, response_code, is_success ? *response_body : std::string(),
      url_loader_->GetFinalURL());
  const bool server_error =
      !is_success || (response_code >= 500 && response_code < 600);
  url_loader_.reset();

  DVLOG(1) << "TimeZoneRequest::OnSimpleLoaderComplete(): timezone={"
           << timezone->ToStringForDebug() << "}";

  const base::Time now = base::Time::Now();
  const bool retry_timeout = (now >= retry_timeout_abs_);

  const bool success = (timezone->status == TimeZoneResponseData::OK);
  if (!success && !retry_timeout) {
    Retry(server_error);
    return;
  }
  RecordUmaResponseTime(base::Time::Now() - request_started_at_, success);

  const TimeZoneRequestResult result =
      (server_error ? TIMEZONE_REQUEST_RESULT_SERVER_ERROR
                    : (success ? TIMEZONE_REQUEST_RESULT_SUCCESS
                               : TIMEZONE_REQUEST_RESULT_FAILURE));
  RecordUmaResult(result, retries_);

  TimeZoneResponseCallback callback = std::move(callback_);

  // Empty callback is used to identify "completed or not yet started request".
  callback_.Reset();

  // callback.Run() usually destroys TimeZoneRequest, because this is the way
  // callback is implemented in TimeZoneProvider.
  std::move(callback).Run(std::move(timezone), server_error);
  // "this" is already destroyed here.
}

std::string TimeZoneResponseData::ToStringForDebug() const {
  static const char* const status2string[] = {
      "OK",
      "INVALID_REQUEST",
      "OVER_QUERY_LIMIT",
      "REQUEST_DENIED",
      "UNKNOWN_ERROR",
      "ZERO_RESULTS",
      "REQUEST_ERROR"
  };

  return base::StringPrintf(
      "dstOffset=%f, rawOffset=%f, timeZoneId='%s', timeZoneName='%s', "
      "error_message='%s', status=%u (%s)",
      dstOffset, rawOffset, timeZoneId.c_str(), timeZoneName.c_str(),
      error_message.c_str(), (unsigned)status,
      (status < std::size(status2string) ? status2string[status] : "unknown"));
}

}  // namespace ash
