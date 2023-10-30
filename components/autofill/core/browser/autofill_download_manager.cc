// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_download_manager.h"

#include <algorithm>
#include <utility>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
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
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_protobufs.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// The reserved identifier ranges for autofill server experiments.
constexpr std::pair<int, int> kAutofillExperimentRanges[] = {
    {3312923, 3312930}, {3314208, 3314209}, {3314711, 3314712},
    {3314445, 3314448}, {3314854, 3314883},
};

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
    "https://content-autofill.googleapis.com/";

// The default number of days after which to reset the registry of autofill
// events for which an upload has been sent.
const base::FeatureParam<int> kAutofillUploadThrottlingPeriodInDays(
    &features::test::kAutofillUploadThrottling,
    switches::kAutofillUploadThrottlingPeriodInDays,
    28);

// Header for API key.
constexpr char kGoogApiKey[] = "X-Goog-Api-Key";
// Header to get base64 encoded serialized proto from API for safety.
constexpr char kGoogEncodeResponseIfExecutable[] =
    "X-Goog-Encode-Response-If-Executable";

// The maximum number of attempts for a given autofill request.
const base::FeatureParam<int> kAutofillMaxServerAttempts(
    &features::test::kAutofillServerCommunication,
    "max-attempts",
    5);

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

  // Server communication is enabled. If there's an autofill server url param
  // use it, otherwise use the default.
  const std::string autofill_server_url_str =
      base::FeatureParam<std::string>(
          &features::test::kAutofillServerCommunication,
          switches::kAutofillServerURL, kDefaultAutofillServerURL)
          .Get();

  GURL autofill_server_url(autofill_server_url_str);

  if (!autofill_server_url.is_valid()) {
    LOG(ERROR) << "Invalid URL param for "
               << features::test::kAutofillServerCommunication.name << "/"
               << switches::kAutofillServerURL << ": "
               << autofill_server_url_str;
    return GURL();
  }

  return autofill_server_url;
}

base::TimeDelta GetThrottleResetPeriod() {
  return base::Days(std::max(1, kAutofillUploadThrottlingPeriodInDays.Get()));
}

// Returns true if |id| is within |kAutofillExperimentRanges|.
bool IsAutofillExperimentId(int id) {
  return base::ranges::any_of(kAutofillExperimentRanges, [id](auto range) {
    const auto& [low, high] = range;
    return low <= id && id <= high;
  });
}

const char* RequestTypeToString(AutofillDownloadManager::RequestType type) {
  switch (type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      return "Query";
    case AutofillDownloadManager::REQUEST_UPLOAD:
      return "Upload";
  }
  NOTREACHED_NORETURN();
}

std::string GetMetricName(AutofillDownloadManager::RequestType request_type,
                          std::string_view suffix) {
  return base::StrCat(
      {"Autofill.", RequestTypeToString(request_type), ".", suffix});
}

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    const AutofillDownloadManager::RequestType& request_type) {
  if (request_type == AutofillDownloadManager::REQUEST_QUERY) {
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
          internal {
            contacts {
              owners: "//components/autofill/OWNERS"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-08-10"
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via 'Enable autofill to "
            "fill out web forms in a single click.' in Chromium's settings "
            "under 'Passwords and forms'. The feature is enabled by default."
          chrome_policy {
            AutofillCreditCardEnabled {
                policy_options {mode: MANDATORY}
                AutofillCreditCardEnabled: false
            }
          }
          chrome_policy {
            AutofillAddressEnabled {
                policy_options {mode: MANDATORY}
                AutofillAddressEnabled: false
            }
          }
          chrome_policy {
            PasswordManagerEnabled {
                policy_options {mode: MANDATORY}
                PasswordManagerEnabled: false
            }
          }
        })");
  }

  DCHECK_EQ(request_type, AutofillDownloadManager::REQUEST_UPLOAD);
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
        internal {
          contacts {
            owners: "//components/autofill/OWNERS"
          }
        }
        user_data {
          type: NONE
        }
        last_reviewed: "2023-07-31"
      }
      policy {
        cookies_allowed: NO
        setting:
          "You can enable or disable this feature via 'Enable autofill to "
          "fill out web forms in a single click.' in Chromium's settings "
          "under 'Passwords and forms'. The feature is enabled by default."
        chrome_policy {
          AutofillCreditCardEnabled {
              policy_options {mode: MANDATORY}
              AutofillCreditCardEnabled: false
          }
        }
        chrome_policy {
          AutofillAddressEnabled {
              policy_options {mode: MANDATORY}
              AutofillAddressEnabled: false
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

std::string FieldTypeToString(uint32_t type) {
  return base::StrCat(
      {base::NumberToString(type), std::string("/"),
       FieldTypeToStringView(ToSafeServerFieldType(type, UNKNOWN_TYPE))});
}

LogBuffer& operator<<(LogBuffer& out, const AutofillPageQueryRequest& query) {
  out << Tag{"div"} << Attrib{"class", "form"};
  out << Tag{"table"};
  out << Tr{} << "client_version:" << query.client_version();
  for (const auto& form : query.forms()) {
    LogBuffer form_buffer(LogBuffer::IsActive(true));
    for (const auto& field : form.fields()) {
      form_buffer << Tag{"table"};
      form_buffer << Tr{} << "Signature"
                  << "Field name"
                  << "Control type";
      form_buffer << Tr{} << field.signature() << field.name()
                  << field.control_type();
      form_buffer << CTag{"table"};
    }
    out << Tr{} << ("Form " + base::NumberToString(form.signature()))
        << std::move(form_buffer);
  }
  out << CTag{"table"};
  out << CTag{"div"};
  return out;
}

LogBuffer& operator<<(LogBuffer& out, const AutofillUploadContents& upload) {
  if (!out.active())
    return out;
  out << Tag{"div"} << Attrib{"class", "form"};
  out << Tag{"table"};
  out << Tr{} << "client_version:" << upload.client_version();
  out << Tr{} << "data_present:" << upload.data_present();
  out << Tr{} << "autofill_used:" << upload.autofill_used();
  out << Tr{} << "submission:" << upload.submission();
  if (upload.has_submission_event()) {
    out << Tr{}
        << "submission_event:" << static_cast<int>(upload.submission_event());
  }
  if (upload.action_signature())
    out << Tr{} << "action_signature:" << upload.action_signature();
  if (upload.login_form_signature())
    out << Tr{} << "login_form_signature:" << upload.login_form_signature();
  if (!upload.form_name().empty())
    out << Tr{} << "form_name:" << upload.form_name();
  if (upload.has_passwords_revealed())
    out << Tr{} << "passwords_revealed:" << upload.passwords_revealed();
  if (upload.has_has_form_tag())
    out << Tr{} << "has_form_tag:" << upload.has_form_tag();

  for (const auto& single_username_data : upload.single_username_data()) {
    LogBuffer single_username_data_buffer(LogBuffer::IsActive(true));
    single_username_data_buffer << Tag{"span"} << "[";
    single_username_data_buffer
        << Tr{} << "username_form_signature:"
        << single_username_data.username_form_signature();
    single_username_data_buffer
        << Tr{} << "username_field_signature:"
        << single_username_data.username_field_signature();
    single_username_data_buffer
        << Tr{}
        << "value_type:" << static_cast<int>(single_username_data.value_type());
    single_username_data_buffer
        << Tr{} << "prompt_edit:"
        << static_cast<int>(single_username_data.prompt_edit());
    out << Tr{} << "single_username_data"
        << std::move(single_username_data_buffer);
  }

  out << Tr{} << "form_signature:" << upload.form_signature();
  for (const auto& field : upload.field()) {
    out << Tr{} << Attrib{"style", "font-weight: bold"}
        << "field_signature:" << field.signature();

    std::vector<std::string> types_as_strings;
    types_as_strings.reserve(field.autofill_type_size());
    for (uint32_t type : field.autofill_type()) {
      types_as_strings.emplace_back(FieldTypeToString(type));
    }
    out << Tr{} << "autofill_type:" << types_as_strings;

    if (!field.name().empty())
      out << Tr{} << "name:" << field.name();
    if (!field.autocomplete().empty())
      out << Tr{} << "autocomplete:" << field.autocomplete();
    if (!field.type().empty())
      out << Tr{} << "type:" << field.type();
    if (field.generation_type()) {
      out << Tr{}
          << "generation_type:" << static_cast<int>(field.generation_type());
    }
  }
  out << CTag{"table"};
  out << CTag{"div"};
  return out;
}

// Returns true if an upload of |form| triggered by |form.submission_source()|
// can be throttled/suppressed. This is true if |prefs| indicates that this
// upload has already happened within the last update window. Updates |prefs|
// account for the upload for |form|.
bool CanThrottleUpload(const FormStructure& form,
                       base::TimeDelta throttle_reset_period,
                       PrefService* pref_service) {
  // PasswordManager uploads are triggered via specific first occurrences and
  // do not participate in the pref-service tracked throttling mechanism. Return
  // false for these uploads.
  if (!pref_service)
    return false;

  // If the upload event pref needs to be reset, clear it now.
  base::Time now = AutofillClock::Now();
  base::Time last_reset =
      pref_service->GetTime(prefs::kAutofillUploadEventsLastResetTimestamp);
  if ((now - last_reset) > throttle_reset_period) {
    AutofillDownloadManager::ClearUploadHistory(pref_service);
  }

  // Get the key for the upload bucket and extract the current bitfield value.
  static constexpr size_t kNumUploadBuckets = 1021;
  std::string key = base::StringPrintf(
      "%03X",
      static_cast<int>(form.form_signature().value() % kNumUploadBuckets));
  const auto& upload_events =
      pref_service->GetDict(prefs::kAutofillUploadEvents);
  int value = upload_events.FindInt(key).value_or(0);

  // Calculate the mask we expect to be set for the form's upload bucket.
  const int bit = static_cast<int>(form.submission_source());
  DCHECK_LE(0, bit);
  DCHECK_LT(bit, 32);
  const int mask = (1 << bit);

  // Check if this is the first upload for this event. If so, update the upload
  // event pref to set the appropriate bit.
  bool is_first_upload_for_event = ((value & mask) == 0);
  if (is_first_upload_for_event) {
    ScopedDictPrefUpdate update(pref_service, prefs::kAutofillUploadEvents);
    update->Set(std::move(key), value | mask);
  }

  return !is_first_upload_for_event;
}

// Determines whether a HTTP request was successful based on its response code.
bool IsHttpSuccess(int response_code) {
  return response_code >= 200 && response_code < 300;
}

absl::optional<std::string> GetUploadPayloadForApi(
    const AutofillUploadContents& upload) {
  AutofillUploadRequest upload_request;
  *upload_request.mutable_upload() = upload;
  std::string payload;
  if (!upload_request.SerializeToString(&payload)) {
    return absl::nullopt;
  }
  return std::move(payload);
}

// Gets an API method URL given its type (query or upload), an optional
// resource ID, and the HTTP method to be used.
// Example usage:
// * GetAPIMethodUrl(REQUEST_QUERY, "1234", "GET") will return "/v1/pages/1234".
// * GetAPIMethodUrl(REQUEST_QUERY, "1234", "POST") will return "/v1/pages:get".
// * GetAPIMethodUrl(REQUEST_UPLOAD, "", "POST") will return "/v1/forms:vote".
std::string GetAPIMethodUrl(AutofillDownloadManager::RequestType type,
                            base::StringPiece resource_id,
                            base::StringPiece method) {
  const char* api_method_url = [&] {
    switch (type) {
      case AutofillDownloadManager::REQUEST_QUERY:
        return method == "POST" ? "/v1/pages:get" : "/v1/pages";
      case AutofillDownloadManager::REQUEST_UPLOAD:
        return "/v1/forms:vote";
    }
    NOTREACHED_NORETURN();
  }();
  if (resource_id.empty()) {
    return std::string(api_method_url);
  }
  return base::StrCat({api_method_url, "/", resource_id});
}

// Gets HTTP body payload for API POST request.
absl::optional<std::string> GetAPIBodyPayload(
    std::string payload,
    AutofillDownloadManager::RequestType type) {
  // Don't do anything for payloads not related to Query.
  if (type != AutofillDownloadManager::REQUEST_QUERY) {
    return std::move(payload);
  }
  // Wrap query payload in a request proto to interface with API Query method.
  AutofillPageResourceQueryRequest request;
  request.set_serialized_request(std::move(payload));
  payload = {};
  if (!request.SerializeToString(&payload)) {
    return absl::nullopt;
  }
  return std::move(payload);
}

// Gets the data payload for API Query (POST and GET).
absl::optional<std::string> GetAPIQueryPayload(
    const AutofillPageQueryRequest& query) {
  std::string serialized_query;
  if (!query.SerializeToString(&serialized_query))
    return absl::nullopt;

  std::string payload;
  base::Base64UrlEncode(serialized_query,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING, &payload);
  return std::move(payload);
}

std::string GetAPIKeyForUrl(version_info::Channel channel) {
  // First look if we can get API key from command line flag.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAutofillAPIKey)) {
    return command_line.GetSwitchValueASCII(switches::kAutofillAPIKey);
  }

  // Get the API key from Chrome baked keys.
  if (channel == version_info::Channel::STABLE) {
    return google_apis::GetAPIKey();
  }
  return google_apis::GetNonStableAPIKey();
}

}  // namespace

struct AutofillDownloadManager::FormRequestData {
  base::WeakPtr<Observer> observer;
  std::vector<FormSignature> form_signatures;
  RequestType request_type;
  absl::optional<net::IsolationInfo> isolation_info;
  std::string payload;
  int num_attempts = 0;
};

ScopedActiveAutofillExperiments::ScopedActiveAutofillExperiments() {
  AutofillDownloadManager::ResetActiveExperiments();
}

ScopedActiveAutofillExperiments::~ScopedActiveAutofillExperiments() {
  AutofillDownloadManager::ResetActiveExperiments();
}

std::vector<variations::VariationID>*
    AutofillDownloadManager::active_experiments_ = nullptr;

AutofillDownloadManager::AutofillDownloadManager(AutofillClient* client,
                                                 version_info::Channel channel,
                                                 LogManager* log_manager)
    : AutofillDownloadManager(client,
                              GetAPIKeyForUrl(channel),
                              log_manager) {}

AutofillDownloadManager::AutofillDownloadManager(AutofillClient* client,
                                                 const std::string& api_key,
                                                 LogManager* log_manager)
    : client_(client),
      api_key_(api_key),
      log_manager_(log_manager),
      autofill_server_url_(GetAutofillServerURL()),
      throttle_reset_period_(GetThrottleResetPeriod()),
      max_form_cache_size_(kAutofillDownloadManagerMaxFormCacheSize),
      loader_backoff_(&kAutofillBackoffPolicy) {}

AutofillDownloadManager::~AutofillDownloadManager() = default;

bool AutofillDownloadManager::IsEnabled() const {
  return autofill_server_url_.is_valid() &&
         base::FeatureList::IsEnabled(
             features::test::kAutofillServerCommunication);
}

bool AutofillDownloadManager::StartQueryRequest(
    const std::vector<FormStructure*>& forms,
    net::IsolationInfo isolation_info,
    base::WeakPtr<Observer> observer) {
  if (!IsEnabled())
    return false;

  // Do not send the request if it contains more fields than the server can
  // accept.
  if (CountActiveFieldsInForms(forms) > kMaxFieldsPerQueryRequest)
    return false;

  // Encode the query for the requested forms.
  AutofillPageQueryRequest query;
  std::vector<FormSignature> queried_form_signatures;
  if (!FormStructure::EncodeQueryRequest(forms, &query,
                                         &queried_form_signatures)) {
    return false;
  }

  // The set of active autofill experiments is constant for the life of the
  // process. We initialize and statically cache it on first use. Leaked on
  // process termination.
  if (active_experiments_ == nullptr)
    InitActiveExperiments();

  // Attach any active autofill experiments.
  query.mutable_experiments()->Reserve(active_experiments_->size());
  for (int id : *active_experiments_)
    query.mutable_experiments()->Add(id);

  absl::optional<std::string> payload = GetAPIQueryPayload(query);
  if (!payload) {
    return false;
  }

  FormRequestData request_data = {
      .observer = observer,
      .form_signatures = std::move(queried_form_signatures),
      .request_type = AutofillDownloadManager::REQUEST_QUERY,
      .isolation_info = std::move(isolation_info),
      .payload = std::move(payload).value(),
  };
  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_SENT);

  std::string query_data;
  if (CheckCacheForQueryRequest(request_data.form_signatures, &query_data)) {
    LOG_AF(log_manager_) << LoggingScope::kAutofillServer
                         << LogMessage::kCachedAutofillQuery << Br{} << query;
    if (request_data.observer) {
      request_data.observer->OnLoadedServerPredictions(
          std::move(query_data), request_data.form_signatures);
    }
    return true;
  }

  LOG_AF(log_manager_) << LoggingScope::kAutofillServer
                       << LogMessage::kSendAutofillQuery << Br{}
                       << "Signatures: " << query;
  return StartRequest(std::move(request_data));
}

bool AutofillDownloadManager::StartUploadRequest(
    const FormStructure& form,
    bool form_was_autofilled,
    const ServerFieldTypeSet& available_field_types,
    const std::string& login_form_signature,
    bool observed_submission,
    PrefService* prefs,
    base::WeakPtr<Observer> observer) {
  if (!IsEnabled())
    return false;

  bool can_throttle_upload =
      CanThrottleUpload(form, throttle_reset_period_, prefs);
  bool throttling_is_enabled =
      base::FeatureList::IsEnabled(features::test::kAutofillUploadThrottling);
  bool is_small_form = form.active_field_count() < 3;
  bool allow_upload =
      !(can_throttle_upload && (throttling_is_enabled || is_small_form));
  AutofillMetrics::LogUploadEvent(form.submission_source(), allow_upload);

  // For debugging purposes, even throttled uploads are logged. If no log
  // manager is active, the function can exit early for throttled uploads.
  bool needs_logging = log_manager_ && log_manager_->IsLoggingActive();
  if (!needs_logging && !allow_upload)
    return false;

  auto Upload = [&](AutofillUploadContents upload) {
    // If this upload was a candidate for throttling, tag it and make sure that
    // any throttling sensitive features are enforced.
    if (can_throttle_upload) {
      upload.set_was_throttleable(true);

      // Don't send randomized metadata.
      upload.clear_randomized_form_metadata();
      for (auto& f : *upload.mutable_field())
        f.clear_randomized_field_metadata();
    }

    // Get the POST payload that contains upload data.
    absl::optional<std::string> payload = GetUploadPayloadForApi(upload);
    if (!payload) {
      return false;
    }

    // If we ever need notification that upload was skipped, add it here.
    if (form.upload_required() == UPLOAD_NOT_REQUIRED) {
      return false;
    }

    FormRequestData request_data = {
        .observer = observer,
        .form_signatures = {form.form_signature()},
        .request_type = AutofillDownloadManager::REQUEST_UPLOAD,
        .isolation_info = absl::nullopt,
        .payload = std::move(payload).value(),
    };

    LOG_AF(log_manager_) << LoggingScope::kAutofillServer
                         << LogMessage::kSendAutofillUpload << Br{}
                         << "Allow upload?: " << allow_upload << Br{}
                         << "Data: " << Br{} << upload;

    if (!allow_upload)
      return false;

    return StartRequest(std::move(request_data));
  };

  std::vector<AutofillUploadContents> uploads =
      form.EncodeUploadRequest(available_field_types, form_was_autofilled,
                               login_form_signature, observed_submission);
  bool all_succeeded = !uploads.empty();
  for (AutofillUploadContents& upload : uploads)
    all_succeeded &= Upload(std::move(upload));
  return all_succeeded;
}

void AutofillDownloadManager::ClearUploadHistory(PrefService* pref_service) {
  if (pref_service) {
    pref_service->ClearPref(prefs::kAutofillUploadEvents);
    pref_service->SetTime(prefs::kAutofillUploadEventsLastResetTimestamp,
                          AutofillClock::Now());
  }
}

size_t AutofillDownloadManager::GetPayloadLength(
    base::StringPiece payload) const {
  return payload.length();
}

std::tuple<GURL, std::string> AutofillDownloadManager::GetRequestURLAndMethod(
    const FormRequestData& request_data) const {
  // ID of the resource to add to the API request URL. Nothing will be added if
  // |resource_id| is empty.
  std::string resource_id;
  std::string method = "POST";

  if (request_data.request_type == AutofillDownloadManager::REQUEST_QUERY) {
    if (GetPayloadLength(request_data.payload) <= kMaxQueryGetSize) {
      resource_id = request_data.payload;
      method = "GET";
      base::UmaHistogramBoolean("Autofill.Query.ApiUrlIsTooLong", false);
    } else {
      base::UmaHistogramBoolean("Autofill.Query.ApiUrlIsTooLong", true);
    }
    base::UmaHistogramBoolean("Autofill.Query.Method", method != "GET");
  }

  // Make the canonical URL to query the API, e.g.,
  // https://autofill.googleapis.com/v1/forms/1234?alt=proto.
  GURL url = autofill_server_url_.Resolve(
      GetAPIMethodUrl(request_data.request_type, resource_id, method));

  // Add the query parameter to set the response format to a serialized proto.
  url = net::AppendQueryParameter(url, "alt", "proto");

  return std::make_tuple(std::move(url), std::move(method));
}

bool AutofillDownloadManager::StartRequest(FormRequestData request_data) {
  // REQUEST_UPLOADs take no IsolationInfo because Password Manager uploads when
  // RenderFrameHostImpl::DidCommitNavigation() is called, in which case
  // AutofillDriver::IsolationInfo() may crash because there is no committing
  // NavigationRequest. Not setting an IsolationInfo is safe because no
  // information about the response is passed to the renderer, or is otherwise
  // visible to a page. See crbug/1176635#c22.
  DCHECK(
      (request_data.request_type == AutofillDownloadManager::REQUEST_UPLOAD) ==
      !request_data.isolation_info);

  // Get the URL and method to use for this request.
  auto [request_url, method] = GetRequestURLAndMethod(request_data);

  // Track the URL length for GET queries because the URL length can be in the
  // thousands when rich metadata is enabled.
  if (request_data.request_type == AutofillDownloadManager::REQUEST_QUERY &&
      method == "GET") {
    base::UmaHistogramCounts100000("Autofill.Query.GetUrlLength",
                                   request_url.spec().length());
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = method;

  // On iOS we have a single, shared URLLoaderFactory provided by BrowserState.
  // As it is shared, it is not trusted and we cannot assign trusted_params
  // to the network request.
#if !BUILDFLAG(IS_IOS)
  if (request_data.isolation_info) {
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    resource_request->trusted_params->isolation_info =
        *request_data.isolation_info;
  }
#endif

  // Add Chrome experiment state to the request headers.
  variations::AppendVariationsHeaderUnknownSignedIn(
      request_url,
      client_->IsOffTheRecord() ? variations::InIncognito::kYes
                                : variations::InIncognito::kNo,
      resource_request.get());

  // Set headers specific to the API.
  // Encode response serialized proto in base64 for safety.
  resource_request->headers.SetHeader(kGoogEncodeResponseIfExecutable,
                                      "base64");

  // Put API key in request's header if a key exists, and the endpoint is
  // trusted by Google.
  if (!api_key_.empty() && request_url.SchemeIs(url::kHttpsScheme) &&
      google_util::IsGoogleAssociatedDomainUrl(request_url)) {
    resource_request->headers.SetHeader(kGoogApiKey, api_key_);
  }

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request),
      GetNetworkTrafficAnnotation(request_data.request_type));

  // This allows reading the error message within the API response when status
  // is not 200 (e.g., 400). Otherwise, URL loader will not give any content in
  // the response when there is a failure, which makes debugging hard.
  simple_loader->SetAllowHttpErrorResults(true);

  if (method == "POST") {
    static constexpr char content_type[] = "application/x-protobuf";
    absl::optional<std::string> payload = GetAPIBodyPayload(
        std::move(request_data.payload), request_data.request_type);
    if (!payload) {
      return false;
    }
    // Attach payload data and add data format header.
    simple_loader->AttachStringForUpload(std::move(payload).value(),
                                         content_type);
  }

  // Transfer ownership of the loader into url_loaders_. Temporarily hang
  // onto the raw pointer to use it as a key and to kick off the request;
  // transferring ownership (std::move) invalidates the |simple_loader|
  // variable.
  auto* raw_simple_loader = simple_loader.get();
  url_loaders_.push_back(std::move(simple_loader));
  raw_simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      client_->GetURLLoaderFactory().get(),
      base::BindOnce(&AutofillDownloadManager::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(--url_loaders_.end()),
                     std::move(request_data), AutofillTickClock::NowTicks()));
  return true;
}

void AutofillDownloadManager::CacheQueryRequest(
    const std::vector<FormSignature>& forms_in_query,
    const std::string& query_data) {
  for (auto it = cached_forms_.begin(); it != cached_forms_.end(); ++it) {
    if (it->first == forms_in_query) {
      // We hit the cache, move to the first position and return.
      auto data = *it;
      cached_forms_.erase(it);
      cached_forms_.push_front(data);
      return;
    }
  }
  cached_forms_.emplace_front(forms_in_query, query_data);
  while (cached_forms_.size() > max_form_cache_size_)
    cached_forms_.pop_back();
}

bool AutofillDownloadManager::CheckCacheForQueryRequest(
    const std::vector<FormSignature>& forms_in_query,
    std::string* query_data) const {
  for (const auto& [signatures, cached_data] : cached_forms_) {
    if (signatures == forms_in_query) {
      // We hit the cache, fill the data and return.
      *query_data = cached_data;
      return true;
    }
  }
  return false;
}

// static
int AutofillDownloadManager::GetMaxServerAttempts() {
  // This value is constant for the life of the browser, so we cache it
  // statically on first use to avoid re-parsing the param on each retry
  // opportunity.
  static const int max_attempts =
      std::clamp(kAutofillMaxServerAttempts.Get(), 1, 20);
  return max_attempts;
}

void AutofillDownloadManager::OnSimpleLoaderComplete(
    std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    FormRequestData request_data,
    base::TimeTicks request_start,
    std::unique_ptr<std::string> response_body) {
  // Move the loader out of the active loaders list.
  std::unique_ptr<network::SimpleURLLoader> simple_loader = std::move(*it);
  url_loaders_.erase(it);

  CHECK(request_data.form_signatures.size() > 0);
  int response_code = -1;  // Invalid response code.
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers) {
    response_code = simple_loader->ResponseInfo()->headers->response_code();
  }

  // We define success as getting 2XX response code and having a response body.
  // Even if the server does not fill the response body when responding, the
  // corresponding response string will be at least instantiated and empty.
  // Having the response body a nullptr probably reflects a problem.
  const bool success = IsHttpSuccess(response_code) && response_body != nullptr;
  loader_backoff_.InformOfRequest(success);

  // Log the HTTP response or error code and request duration.
  int net_error = simple_loader->NetError();
  base::UmaHistogramSparse(
      GetMetricName(request_data.request_type, "HttpResponseOrErrorCode"),
      net_error != net::OK && net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE
          ? net_error
          : response_code);
  base::UmaHistogramTimes(
      GetMetricName(request_data.request_type, "RequestDuration"),
      AutofillTickClock::NowTicks() - request_start);

  // Handle error if there is and return.
  if (!success) {
    std::string error_message =
        (response_body != nullptr) ? *response_body : "";
    DVLOG(1) << "AutofillDownloadManager: "
             << RequestTypeToString(request_data.request_type)
             << " request has failed with net error "
             << simple_loader->NetError() << " and HTTP response code "
             << response_code << " and error message from the server "
             << error_message;
    base::UmaHistogramCounts100000(
        GetMetricName(request_data.request_type, "FailingPayloadSize"),
        request_data.payload.length());

    if (request_data.observer) {
      request_data.observer->OnServerRequestError(
          request_data.form_signatures.front(), request_data.request_type,
          response_code);
    }

    // If the failure was a client error don't retry.
    if (response_code >= 400 && response_code <= 499) {
      return;
    }

    // If we've exhausted the maximum number of attempts, don't retry.
    if (++request_data.num_attempts >= GetMaxServerAttempts()) {
      return;
    }

    // Reschedule with the appropriate delay, ignoring return value because
    // payload is already well formed.
    base::TimeDelta backoff = loader_backoff_.GetTimeUntilRelease();
    base::UmaHistogramMediumTimes(
        GetMetricName(request_data.request_type, "BackoffDelay"), backoff);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&AutofillDownloadManager::StartRequest),
            weak_factory_.GetWeakPtr(), std::move(request_data)),
        backoff);
    return;
  }

  switch (request_data.request_type) {
    case REQUEST_QUERY: {
      CacheQueryRequest(request_data.form_signatures, *response_body);
      base::UmaHistogramBoolean("Autofill.Query.WasInCache",
                                simple_loader->LoadedFromCache());
      if (request_data.observer) {
        request_data.observer->OnLoadedServerPredictions(
            std::move(*response_body), request_data.form_signatures);
      }
      return;
    }
    case REQUEST_UPLOAD:
      DVLOG(1) << "AutofillDownloadManager: upload request has succeeded.";
      if (request_data.observer) {
        request_data.observer->OnUploadedPossibleFieldTypes();
      }
      return;
  }
  NOTREACHED_NORETURN();
}

void AutofillDownloadManager::InitActiveExperiments() {
  auto* variations_ids_provider =
      variations::VariationsIdsProvider::GetInstance();
  DCHECK(variations_ids_provider != nullptr);

  // TODO(crbug.com/1331322): Retire the hardcoded GWS ID ranges and only read
  // the finch parameter.
  base::flat_set<int> active_experiments(
      variations_ids_provider->GetVariationsVector(
          {variations::GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
           variations::GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));
  base::EraseIf(active_experiments, base::not_fn(&IsAutofillExperimentId));
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillServerBehaviors)) {
    active_experiments.insert(
        autofill::features::kAutofillServerBehaviorsParam.Get());
  }

  delete active_experiments_;
  active_experiments_ = new std::vector<int>(active_experiments.begin(),
                                             active_experiments.end());
  std::sort(active_experiments_->begin(), active_experiments_->end());
}

// static
void AutofillDownloadManager::ResetActiveExperiments() {
  delete active_experiments_;
  active_experiments_ = nullptr;
}

}  // namespace autofill
