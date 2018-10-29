// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_download_manager.h"

#include <tuple>
#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/submission_source.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {

namespace {

const size_t kMaxQueryGetSize = 1400;  // 1.25KB
const size_t kAutofillDownloadManagerMaxFormCacheSize = 16;
const size_t kMaxFieldsPerQueryRequest = 100;

const net::BackoffEntry::Policy kAutofillBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    1000,  // 1 second.

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.33,  // 33%.

    // Maximum amount of time we are willing to delay our request in ms.
    30 * 1000,  // 30 seconds.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

const char kDefaultAutofillServerURL[] =
    "https://clients1.google.com/tbproxy/af/";

// Returns the base URL for the autofill server.
GURL GetAutofillServerURL() {
  // If a valid autofill server URL is specified on the command line, then the
  // AutofillDownlaodManager will use it, and assume that server communication
  // is enabled.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAutofillServerURL)) {
    GURL url(command_line.GetSwitchValueASCII(switches::kAutofillServerURL));
    if (url.is_valid())
      return url;

    LOG(ERROR) << "Invalid URL value for --" << switches::kAutofillServerURL
               << ": "
               << command_line.GetSwitchValueASCII(
                      switches::kAutofillServerURL);
  }

  // If communication is disabled, leave the autofill server URL unset.
  if (!base::FeatureList::IsEnabled(features::kAutofillServerCommunication))
    return GURL();

  // Server communication is enabled. If there's an autofill server url param
  // use it, otherwise use the default.
  const std::string autofill_server_url_str =
      base::FeatureParam<std::string>(&features::kAutofillServerCommunication,
                                      switches::kAutofillServerURL,
                                      kDefaultAutofillServerURL)
          .Get();

  GURL autofill_server_url(autofill_server_url_str);

  if (!autofill_server_url.is_valid()) {
    LOG(ERROR) << "Invalid URL param for "
               << features::kAutofillServerCommunication.name << "/"
               << switches::kAutofillServerURL << ": "
               << autofill_server_url_str;
    return GURL();
  }

  return autofill_server_url;
}

// Helper to log the HTTP |response_code| and other data received for
// |request_type| to UMA.
void LogHttpResponseData(AutofillDownloadManager::RequestType request_type,
                         int response_code,
                         int net_error,
                         base::TimeDelta request_duration) {
  int response_or_error_code =
      (net_error == net::OK || net_error == net::ERR_FAILED) ? response_code
                                                             : net_error;
  switch (request_type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      base::UmaHistogramSparse("Autofill.Query.HttpResponseOrErrorCode",
                               response_or_error_code);
      UMA_HISTOGRAM_TIMES("Autofill.Query.RequestDuration", request_duration);
      break;
    case AutofillDownloadManager::REQUEST_UPLOAD:
      base::UmaHistogramSparse("Autofill.Upload.HttpResponseOrErrorCode",
                               response_or_error_code);
      UMA_HISTOGRAM_TIMES("Autofill.Upload.RequestDuration", request_duration);
      break;
    default:
      NOTREACHED();
      base::UmaHistogramSparse("Autofill.Unknown.HttpResponseOrErrorCode",
                               response_or_error_code);
      UMA_HISTOGRAM_TIMES("Autofill.Unknown.RequestDuration", request_duration);
  }
}

// Helper to log, to UMA, the |num_bytes| sent for a failing instance of
// |request_type|.
void LogFailingPayloadSize(AutofillDownloadManager::RequestType request_type,
                           size_t num_bytes) {
  switch (request_type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      UMA_HISTOGRAM_COUNTS_100000("Autofill.Query.FailingPayloadSize",
                                  num_bytes);
      break;
    case AutofillDownloadManager::REQUEST_UPLOAD:
      UMA_HISTOGRAM_COUNTS_100000("Autofill.Upload.FailingPayloadSize",
                                  num_bytes);
      break;
    default:
      NOTREACHED();
      UMA_HISTOGRAM_COUNTS_100000("Autofill.Unknown.FailingPayloadSize",
                                  num_bytes);
  }
}

// Helper to log, to UMA, the |delay| caused by exponential backoff.
void LogExponentialBackoffDelay(
    AutofillDownloadManager::RequestType request_type,
    base::TimeDelta delay) {
  switch (request_type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      UMA_HISTOGRAM_MEDIUM_TIMES("Autofill.Query.BackoffDelay", delay);
      break;
    case AutofillDownloadManager::REQUEST_UPLOAD:
      UMA_HISTOGRAM_MEDIUM_TIMES("Autofill.Upload.BackoffDelay", delay);
      break;
    default:
      NOTREACHED();
      UMA_HISTOGRAM_MEDIUM_TIMES("Autofill.Unknown.BackoffDelay", delay);
  }
}

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    const autofill::AutofillDownloadManager::RequestType& request_type) {
  if (request_type == autofill::AutofillDownloadManager::REQUEST_QUERY) {
    return net::DefineNetworkTrafficAnnotation("autofill_query", R"(
        semantics {
          sender: "Autofill"
          description:
            "Chromium can automatically fill in web forms. If the feature is "
            "enabled, Chromium will send a non-identifying description of the "
            "form to Google's servers, which will respond with the type of "
            "data required by each of the form's fields, if known. I.e., if a "
            "field expects to receive a name, phone number, street address, "
            "etc."
          trigger: "User encounters a web form."
          data:
            "Hashed descriptions of the form and its fields. User data is not "
            "sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via 'Enable autofill to "
            "fill out web forms in a single click.' in Chromium's settings "
            "under 'Passwords and forms'. The feature is enabled by default."
          chrome_policy {
            AutoFillEnabled {
                policy_options {mode: MANDATORY}
                AutoFillEnabled: false
            }
          }
        })");
  }

  DCHECK_EQ(request_type, autofill::AutofillDownloadManager::REQUEST_UPLOAD);
  return net::DefineNetworkTrafficAnnotation("autofill_upload", R"(
      semantics {
        sender: "Autofill"
        description:
          "Chromium relies on crowd-sourced field type classifications to "
          "help it automatically fill in web forms. If the feature is "
          "enabled, Chromium will send a non-identifying description of the "
          "form to Google's servers along with the type of data Chromium "
          "observed being given to the form. I.e., if you entered your first "
          "name into a form field, Chromium will 'vote' for that form field "
          "being a first name field."
        trigger: "User submits a web form."
        data:
          "Hashed descriptions of the form and its fields along with type of "
          "data given to each field, if recognized from the user's "
          "profile(s). User data is not sent."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "You can enable or disable this feature via 'Enable autofill to "
          "fill out web forms in a single click.' in Chromium's settings "
          "under 'Passwords and forms'. The feature is enabled by default."
        chrome_policy {
          AutoFillEnabled {
              policy_options {mode: MANDATORY}
              AutoFillEnabled: false
          }
        }
      })");
}

size_t CountActiveFieldsInForms(const std::vector<FormStructure*>& forms) {
  size_t active_field_count = 0;
  for (const auto* form : forms)
    active_field_count += form->active_field_count();
  return active_field_count;
}

const char* RequestTypeToString(AutofillDownloadManager::RequestType type) {
  switch (type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      return "query";
    case AutofillDownloadManager::REQUEST_UPLOAD:
      return "upload";
  }
  NOTREACHED();
  return "";
}

std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillQueryContents& query) {
  out << "client_version: " << query.client_version();
  for (const auto& form : query.form()) {
    out << "\nForm\n signature: " << form.signature();
    for (const auto& field : form.field()) {
      out << "\n Field\n  signature: " << field.signature();
      if (!field.name().empty())
        out << "\n  name: " << field.name();
      if (!field.type().empty())
        out << "\n  type: " << field.type();
    }
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillUploadContents& upload) {
  out << "client_version: " << upload.client_version() << "\n";
  out << "form_signature: " << upload.form_signature() << "\n";
  out << "data_present: " << upload.data_present() << "\n";
  out << "submission: " << upload.submission() << "\n";
  if (!upload.action_signature())
    out << "action_signature: " << upload.action_signature() << "\n";
  if (!upload.login_form_signature())
    out << "login_form_signature: " << upload.login_form_signature() << "\n";
  if (!upload.form_name().empty())
    out << "form_name: " << upload.form_name() << "\n";

  for (const auto& field : upload.field()) {
    out << "\n Field"
        << "\n signature: " << field.signature() << "\n autofill_type: [";
    for (int i = 0; i < field.autofill_type_size(); ++i) {
      if (i)
        out << ", ";
      out << field.autofill_type(i);
    }
    out << "]";

    out << "\n (autofill_type, validity_states): [";
    for (const auto& type_validities : field.autofill_type_validities()) {
      out << "(type: " << type_validities.type() << ", validities: {";
      for (int i = 0; i < type_validities.validity_size(); ++i) {
        if (i)
          out << ", ";
        out << type_validities.validity(i);
      }
      out << "})";
    }
    out << "]\n";
    if (!field.name().empty())
      out << "\n name: " << field.name();
    if (!field.autocomplete().empty())
      out << "\n autocomplete: " << field.autocomplete();
    if (!field.type().empty())
      out << "\n type: " << field.type();
    if (field.generation_type())
      out << "\n generation_type: " << field.generation_type();
  }
  return out;
}

// Check for and returns true if |upload_event| is allowed to trigger an upload
// for |form|. If true, updates |prefs| to track that |upload_event| has been
// recorded for |form|.
bool IsUploadAllowed(const FormStructure& form, PrefService* pref_service) {
  if (!pref_service ||
      !base::FeatureList::IsEnabled(features::kAutofillUploadThrottling)) {
    return true;
  }

  // If the upload event pref needs to be reset, clear it now.
  static constexpr base::TimeDelta kResetPeriod = base::TimeDelta::FromDays(28);
  base::Time now = AutofillClock::Now();
  base::Time last_reset =
      pref_service->GetTime(prefs::kAutofillUploadEventsLastResetTimestamp);
  if ((now - last_reset) > kResetPeriod) {
    AutofillDownloadManager::ClearUploadHistory(pref_service);
  }

  // Get the key for the upload bucket and extract the current bitfield value.
  static constexpr size_t kNumUploadBuckets = 1021;
  std::string key = base::StringPrintf(
      "%03X", static_cast<int>(form.form_signature() % kNumUploadBuckets));
  auto* upload_events =
      pref_service->GetDictionary(prefs::kAutofillUploadEvents);
  auto* found = upload_events->FindKeyOfType(key, base::Value::Type::INTEGER);
  int value = found ? found->GetInt() : 0;

  // Calculate the mask we expect to be set for the form's upload bucket.
  const int bit = static_cast<int>(form.submission_source());
  DCHECK_LE(0, bit);
  DCHECK_LT(bit, 32);
  const int mask = (1 << bit);

  // Check if the upload should be allowed and, if so, update the upload event
  // pref to set the appropriate bit.
  bool allow_upload = ((value & mask) == 0);
  if (allow_upload) {
    DictionaryPrefUpdate update(pref_service, prefs::kAutofillUploadEvents);
    update->SetKey(std::move(key), base::Value(value | mask));
  }

  // Capture metrics and return.
  AutofillMetrics::LogUploadEvent(form.submission_source(), allow_upload);
  return allow_upload;
}

}  // namespace

struct AutofillDownloadManager::FormRequestData {
  std::vector<std::string> form_signatures;
  RequestType request_type;
  std::string payload;
};

AutofillDownloadManager::AutofillDownloadManager(AutofillDriver* driver,
                                                 Observer* observer)
    : driver_(driver),
      observer_(observer),
      autofill_server_url_(GetAutofillServerURL()),
      max_form_cache_size_(kAutofillDownloadManagerMaxFormCacheSize),
      loader_backoff_(&kAutofillBackoffPolicy),
      weak_factory_(this) {
  DCHECK(observer_);
}

AutofillDownloadManager::~AutofillDownloadManager() = default;

bool AutofillDownloadManager::StartQueryRequest(
    const std::vector<FormStructure*>& forms) {
  if (!IsEnabled())
    return false;

  // Do not send the request if it contains more fields than the server can
  // accept.
  if (CountActiveFieldsInForms(forms) > kMaxFieldsPerQueryRequest)
    return false;

  AutofillQueryContents query;
  FormRequestData request_data;
  if (!FormStructure::EncodeQueryRequest(forms, &request_data.form_signatures,
                                         &query)) {
    return false;
  }

  std::string payload;
  if (!query.SerializeToString(&payload))
    return false;

  request_data.request_type = AutofillDownloadManager::REQUEST_QUERY;
  request_data.payload = std::move(payload);
  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_SENT);

  std::string query_data;
  if (CheckCacheForQueryRequest(request_data.form_signatures, &query_data)) {
    DVLOG(1) << "AutofillDownloadManager: query request has been retrieved "
             << "from the cache, form signatures: "
             << GetCombinedSignature(request_data.form_signatures);
    observer_->OnLoadedServerPredictions(std::move(query_data),
                                         request_data.form_signatures);
    return true;
  }

  DVLOG(1) << "Sending Autofill Query Request:\n" << query;

  return StartRequest(std::move(request_data));
}

bool AutofillDownloadManager::StartUploadRequest(
    const FormStructure& form,
    bool form_was_autofilled,
    const ServerFieldTypeSet& available_field_types,
    const std::string& login_form_signature,
    bool observed_submission,
    PrefService* prefs) {
  if (!IsEnabled() || !IsUploadAllowed(form, prefs))
    return false;

  AutofillUploadContents upload;
  if (!form.EncodeUploadRequest(available_field_types, form_was_autofilled,
                                login_form_signature, observed_submission,
                                &upload))
    return false;

  std::string payload;
  if (!upload.SerializeToString(&payload))
    return false;

  if (form.upload_required() == UPLOAD_NOT_REQUIRED) {
    DVLOG(1) << "AutofillDownloadManager: Upload request is ignored.";
    // If we ever need notification that upload was skipped, add it here.
    return false;
  }

  FormRequestData request_data;
  request_data.form_signatures.push_back(form.FormSignatureAsStr());
  request_data.request_type = AutofillDownloadManager::REQUEST_UPLOAD;
  request_data.payload = std::move(payload);

  DVLOG(1) << "Sending Autofill Upload Request:\n" << upload;

  return StartRequest(std::move(request_data));
}

void AutofillDownloadManager::ClearUploadHistory(PrefService* pref_service) {
  if (pref_service) {
    pref_service->ClearPref(prefs::kAutofillUploadEvents);
    pref_service->SetTime(prefs::kAutofillUploadEventsLastResetTimestamp,
                          AutofillClock::Now());
  }
}

std::tuple<GURL, std::string> AutofillDownloadManager::GetRequestURLAndMethod(
    const FormRequestData& request_data) const {
  std::string method("POST");
  std::string query_str;

  if (request_data.request_type == AutofillDownloadManager::REQUEST_QUERY) {
    if (request_data.payload.length() <= kMaxQueryGetSize &&
        base::FeatureList::IsEnabled(features::kAutofillCacheQueryResponses)) {
      method = "GET";
      std::string base64_payload;
      base::Base64UrlEncode(request_data.payload,
                            base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                            &base64_payload);
      base::StrAppend(&query_str, {"q=", base64_payload});
    }
    UMA_HISTOGRAM_BOOLEAN("Autofill.Query.Method", (method == "GET") ? 0 : 1);
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query_str);

  GURL url = autofill_server_url_
                 .Resolve(RequestTypeToString(request_data.request_type))
                 .ReplaceComponents(replacements);
  return std::make_tuple(std::move(url), std::move(method));
}

bool AutofillDownloadManager::StartRequest(FormRequestData request_data) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      driver_->GetURLLoaderFactory();
  DCHECK(url_loader_factory);

  // Get the URL and method to use for this request.
  std::string method;
  GURL request_url;
  std::tie(request_url, method) = GetRequestURLAndMethod(request_data);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->method = method;

  // Add Chrome experiment state to the request headers.
  variations::AppendVariationHeadersUnknownSignedIn(
      request_url,
      driver_->IsIncognito() ? variations::InIncognito::kYes
                             : variations::InIncognito::kNo,
      &resource_request->headers);

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request),
      GetNetworkTrafficAnnotation(request_data.request_type));
  if (method == "POST")
    simple_loader->AttachStringForUpload(request_data.payload, "text/proto");

  // Transfer ownership of the loader into url_loaders_. Temporarily hang
  // onto the raw pointer to use it as a key and to kick off the request;
  // transferring ownership (std::move) invalidates the |simple_loader|
  // variable.
  auto* raw_simple_loader = simple_loader.get();
  url_loaders_.push_back(std::move(simple_loader));
  raw_simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(&AutofillDownloadManager::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(--url_loaders_.end()),
                     std::move(request_data), base::TimeTicks::Now()));
  return true;
}

void AutofillDownloadManager::CacheQueryRequest(
    const std::vector<std::string>& forms_in_query,
    const std::string& query_data) {
  std::string signature = GetCombinedSignature(forms_in_query);
  for (auto it = cached_forms_.begin(); it != cached_forms_.end(); ++it) {
    if (it->first == signature) {
      // We hit the cache, move to the first position and return.
      std::pair<std::string, std::string> data = *it;
      cached_forms_.erase(it);
      cached_forms_.push_front(data);
      return;
    }
  }
  std::pair<std::string, std::string> data;
  data.first = signature;
  data.second = query_data;
  cached_forms_.push_front(data);
  while (cached_forms_.size() > max_form_cache_size_)
    cached_forms_.pop_back();
}

bool AutofillDownloadManager::CheckCacheForQueryRequest(
    const std::vector<std::string>& forms_in_query,
    std::string* query_data) const {
  std::string signature = GetCombinedSignature(forms_in_query);
  for (const auto& it : cached_forms_) {
    if (it.first == signature) {
      // We hit the cache, fill the data and return.
      *query_data = it.second;
      return true;
    }
  }
  return false;
}

std::string AutofillDownloadManager::GetCombinedSignature(
    const std::vector<std::string>& forms_in_query) const {
  size_t total_size = forms_in_query.size();
  for (size_t i = 0; i < forms_in_query.size(); ++i)
    total_size += forms_in_query[i].length();
  std::string signature;

  signature.reserve(total_size);

  for (size_t i = 0; i < forms_in_query.size(); ++i) {
    if (i)
      signature.append(",");
    signature.append(forms_in_query[i]);
  }
  return signature;
}

void AutofillDownloadManager::OnSimpleLoaderComplete(
    std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    FormRequestData request_data,
    base::TimeTicks request_start,
    std::unique_ptr<std::string> response_body) {
  // Move the loader out of the active loaders list.
  std::unique_ptr<network::SimpleURLLoader> simple_loader = std::move(*it);
  url_loaders_.erase(it);

  CHECK(request_data.form_signatures.size());
  // net:ERR_FAILED is not an HTTP response code, but if none is available, the
  // UMA logging can accept this as a generic fallback as well.
  int response_code = net::ERR_FAILED;
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers) {
    response_code = simple_loader->ResponseInfo()->headers->response_code();
  }

  const bool success = !!response_body;
  loader_backoff_.InformOfRequest(success);

  LogHttpResponseData(request_data.request_type, response_code,
                      simple_loader->NetError(),
                      base::TimeTicks::Now() - request_start);

  if (!success) {
    DVLOG(1) << "AutofillDownloadManager: "
             << RequestTypeToString(request_data.request_type)
             << " request has failed with response " << response_code;

    observer_->OnServerRequestError(request_data.form_signatures[0],
                                    request_data.request_type, response_code);

    LogFailingPayloadSize(request_data.request_type,
                          request_data.payload.length());

    // If the failure was a client error don't retry.
    if (response_code >= 400 && response_code <= 499)
      return;

    base::TimeDelta backoff = loader_backoff_.GetTimeUntilRelease();
    LogExponentialBackoffDelay(request_data.request_type, backoff);

    // Reschedule with the appropriate delay, ignoring return value because
    // payload is already well formed.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&AutofillDownloadManager::StartRequest),
            weak_factory_.GetWeakPtr(), std::move(request_data)),
        backoff);
    return;
  }

  if (request_data.request_type == AutofillDownloadManager::REQUEST_QUERY) {
    CacheQueryRequest(request_data.form_signatures, *response_body);
    UMA_HISTOGRAM_BOOLEAN("Autofill.Query.WasInCache",
                          simple_loader->LoadedFromCache());
    observer_->OnLoadedServerPredictions(std::move(*response_body),
                                         request_data.form_signatures);
    return;
  }

  DCHECK_EQ(request_data.request_type, AutofillDownloadManager::REQUEST_UPLOAD);
  DVLOG(1) << "AutofillDownloadManager: upload request has succeeded.";
  observer_->OnUploadedPossibleFieldTypes();
}

}  // namespace autofill
