// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_protobufs.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_ids_provider.h"
#include "google_apis/common/api_key_request_util.h"
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

constexpr size_t kAutofillCrowdsourcingManagerMaxFormCacheSize = 16;
constexpr size_t kMaxFieldsPerQueryRequest = 100;

constexpr base::TimeDelta kFetchTimeout(base::Seconds(10));

constexpr net::BackoffEntry::Policy kAutofillBackoffPolicy = {
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

constexpr char kDefaultAutofillServerURL[] =
    "https://content-autofill.googleapis.com/";

// Header to get base64 encoded serialized proto from API for safety.
constexpr char kGoogEncodeResponseIfExecutable[] =
    "X-Goog-Encode-Response-If-Executable";

// The default number of days after which to reset the registry of autofill
// events for which an upload has been sent.
const base::FeatureParam<int> kAutofillUploadThrottlingPeriodInDays(
    &features::test::kAutofillUploadThrottling,
    switches::kAutofillUploadThrottlingPeriodInDays,
    28);

// The maximum number of attempts for a given autofill request.
const base::FeatureParam<int> kAutofillMaxServerAttempts(
    &features::test::kAutofillServerCommunication,
    "max-attempts",
    5);

enum class RequestType {
  kRequestQuery,
  kRequestUpload,
};

// Used in `ShouldThrottleUpload` to specify which part of the upload is
// checked for throttling.
enum class UploadType {
  kVote,
  kMetadata,
};

// Returns the base URL for the autofill server.
GURL GetAutofillServerURL() {
  // If a valid autofill server URL is specified on the command line, then the
  // AutofillCrowdsourcingManager will use it, and assume that server
  // communication is enabled.
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

// Returns true if `id` is within `kAutofillExperimentRanges`.
bool IsAutofillExperimentId(int id) {
  return std::ranges::any_of(kAutofillExperimentRanges, [id](auto range) {
    const auto& [low, high] = range;
    return low <= id && id <= high;
  });
}

std::string GetMetricName(RequestType request_type, std::string_view suffix) {
  auto TypeToName = [](RequestType type) -> std::string_view {
    switch (type) {
      case RequestType::kRequestQuery:
        return "Query";
      case RequestType::kRequestUpload:
        return "Upload";
    }
    NOTREACHED();
  };
  return base::StrCat({"Autofill.", TypeToName(request_type), ".", suffix});
}

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kRequestQuery:
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
    case RequestType::kRequestUpload:
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
  NOTREACHED();
}

size_t CountActiveFieldsInForms(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  size_t active_field_count = 0;
  for (const autofill::FormStructure* form : forms) {
    active_field_count += form->active_field_count();
  }
  return active_field_count;
}

std::string FieldTypeToString(uint32_t type) {
  return base::StrCat(
      {base::NumberToString(type), std::string("/"),
       FieldTypeToStringView(ToSafeFieldType(type, UNKNOWN_TYPE))});
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
  for (const auto& field : upload.field_data()) {
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
    if (field.has_initial_value_changed()) {
      out << Tr{} << "initial_value_changed:"
          << static_cast<int>(field.initial_value_changed());
    }
  }
  out << CTag{"table"};
  out << CTag{"div"};
  return out;
}

// Returns true if part of upload of a form with `form_signature`, triggered by
// `form_submission_source` should be throttled/suppressed. This is true if
// `pref_service` indicates that this upload has already happened within the
// last update window. Updates `pref_service` account for the upload of a form
// with `form_signature`.
// If `upload_type` equals `UploadType::kVote`, the check is done on the vote
// part of the upload. Vote throttling is only used on the Autofill side.
// If `upload_type` equals `UploadType::kMetadata` the check is done on the
// metadata part of the upload. Metadata throttling is shared by Autofill and
// the Password Manager, ensuring that together they don't upload metadata more
// frequently than desired.
bool ShouldThrottleUpload(FormSignature form_signature,
                          UploadType upload_type,
                          base::TimeDelta throttle_reset_period,
                          PrefService* pref_service,
                          std::optional<mojom::SubmissionSource>
                              form_submission_source_for_vote_upload) {
  // `form_submission_source_for_vote_upload` must be set only on vote uploads.
  CHECK(upload_type == UploadType::kMetadata ||
        form_submission_source_for_vote_upload.has_value());
  CHECK(pref_service);
  // If the upload event pref needs to be reset, clear it now.
  base::Time now = AutofillClock::Now();
  base::Time last_reset =
      pref_service->GetTime(prefs::kAutofillUploadEventsLastResetTimestamp);
  if ((now - last_reset) > throttle_reset_period) {
    AutofillCrowdsourcingManager::ClearUploadHistory(pref_service);
  }

  std::string_view preference = upload_type == UploadType::kVote
                                    ? prefs::kAutofillVoteUploadEvents
                                    : prefs::kAutofillMetadataUploadEvents;

  // Get the key for the upload bucket and extract the current bitfield value.
  static constexpr size_t kNumUploadBuckets = 1021;
  std::string key = base::StringPrintf(
      "%03X", static_cast<int>(form_signature.value() % kNumUploadBuckets));
  int value = pref_service->GetDict(preference).FindInt(key).value_or(0);

  // Calculate the mask we expect to be set for the form's upload bucket.
  int mask = 0;
  switch (upload_type) {
    case UploadType::kVote: {
      const int bit = static_cast<int>(*form_submission_source_for_vote_upload);
      DCHECK_LE(0, bit);
      DCHECK_LT(bit, 32);
      mask = (1 << bit);
      break;
    }
    case UploadType::kMetadata:
      mask = 1;
      break;
  }

  // Check if this is the first upload for this event. If so, update the upload
  // event pref to set the appropriate bit.
  const bool is_first_upload_for_event = ((value & mask) == 0);
  if (is_first_upload_for_event) {
    ScopedDictPrefUpdate update(pref_service, std::string(preference));
    update->Set(key, value | mask);
  }

  return !is_first_upload_for_event;
}

// Determines whether a HTTP request was successful based on its response code.
bool IsHttpSuccess(int response_code) {
  return response_code >= 200 && response_code < 300;
}

std::optional<std::string> GetUploadPayloadForApi(
    const AutofillUploadContents& upload) {
  AutofillUploadRequest upload_request;
  *upload_request.mutable_upload() = upload;
  std::string payload;
  if (!upload_request.SerializeToString(&payload)) {
    return std::nullopt;
  }
  return std::move(payload);
}

// Gets an API method URL given its type (query or upload), an optional
// resource ID, and the HTTP method to be used.
// Example usage:
// * GetAPIMethodUrl(RequestType::kRequestQuery, "1234", "GET") will return
//   "/v1/pages/1234".
// * GetAPIMethodUrl(RequestType::kRequestQuery, "1234", "POST") will return
//   "/v1/pages:get".
// * GetAPIMethodUrl(RequestType::kRequestUpload, "", "POST") will return
//   "/v1/forms:vote".
std::string GetAPIMethodUrl(RequestType type,
                            std::string_view resource_id,
                            std::string_view method) {
  const char* api_method_url = [&] {
    switch (type) {
      case RequestType::kRequestQuery:
        return method == "POST" ? "/v1/pages:get" : "/v1/pages";
      case RequestType::kRequestUpload:
        return "/v1/forms:vote";
    }
    NOTREACHED();
  }();
  if (resource_id.empty()) {
    return std::string(api_method_url);
  }
  return base::StrCat({api_method_url, "/", resource_id});
}

// Gets HTTP body payload for API POST request.
std::optional<std::string> GetAPIBodyPayload(std::string payload,
                                             RequestType type) {
  // Don't do anything for payloads not related to Query.
  if (type != RequestType::kRequestQuery) {
    return std::move(payload);
  }
  // Wrap query payload in a request proto to interface with API Query method.
  AutofillPageResourceQueryRequest request;
  request.set_serialized_request(std::move(payload));
  payload = {};
  if (!request.SerializeToString(&payload)) {
    return std::nullopt;
  }
  return std::move(payload);
}

// Gets the data payload for API Query (POST and GET).
std::optional<std::string> GetAPIQueryPayload(
    const AutofillPageQueryRequest& query) {
  std::string serialized_query;
  if (!query.SerializeToString(&serialized_query))
    return std::nullopt;

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
  return google_apis::GetAPIKey(channel);
}

std::optional<std::vector<variations::VariationID>>& GetActiveExperiments() {
  static base::NoDestructor<std::optional<std::vector<variations::VariationID>>>
      active_experiments;
  return *active_experiments;
}

// Populates `GetActiveExperiments()` with the set of active autofill server
// experiments.
void InitActiveExperiments() {
  auto* variations_ids_provider =
      variations::VariationsIdsProvider::GetInstance();
  DCHECK(variations_ids_provider != nullptr);

  // TODO(crbug.com/40227501): Retire the hardcoded GWS ID ranges and only read
  // the finch parameter.
  std::vector<variations::VariationID> active_experiments =
      variations_ids_provider->GetVariationsVector(
          {variations::GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
           variations::GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY});
  std::erase_if(active_experiments, std::not_fn(&IsAutofillExperimentId));
  std::sort(active_experiments.begin(), active_experiments.end());
  active_experiments.erase(
      std::unique(active_experiments.begin(), active_experiments.end()),
      active_experiments.end());

  GetActiveExperiments() = std::move(active_experiments);
}

}  // namespace

template <typename Signature>
class ScopedCallbackRunner;

// A variant of `base::ScopedClosureRunner` that encapsulates a callback and
// default arguments.
template <typename R, typename... Args>
class ScopedCallbackRunner<R(Args...)> final {
 public:
  ScopedCallbackRunner() = default;

  [[nodiscard]] explicit ScopedCallbackRunner(
      base::OnceCallback<R(Args...)> callback,
      Args&&... args)
      : callback_(std::move(callback)), args_(std::forward<Args>(args)...) {}

  ScopedCallbackRunner(ScopedCallbackRunner&& other) = default;

  ScopedCallbackRunner& operator=(ScopedCallbackRunner&& other) {
    if (this != &other) {
      RunAndReset();
      callback_ = std::move(other.callback_);
      args_ = std::move(other.args_);
    }
    return *this;
  }

  ~ScopedCallbackRunner() { RunAndReset(); }

  explicit operator bool() const { return !!callback_; }

  void RunAndReset() {
    if (callback_) {
      [&]<size_t... Indexes>(std::index_sequence<Indexes...>) {
        std::move(callback_).Run(std::get<Indexes>(std::move(args_))...);
      }(std::make_index_sequence<sizeof...(Args)>());
      DCHECK(!callback_);
    }
  }

  [[nodiscard]] base::OnceCallback<R(Args...)> Release() && {
    return std::move(callback_);
  }

 private:
  base::OnceCallback<R(Args...)> callback_;
  std::tuple<Args...> args_;
};

struct AutofillCrowdsourcingManager::FormRequestData {
  ScopedCallbackRunner<void(std::optional<QueryResponse>)> callback;
  std::vector<FormSignature> form_signatures;
  RequestType request_type;
  std::optional<net::IsolationInfo> isolation_info;
  std::string payload;
  int num_attempts = 0;
};

ScopedActiveAutofillExperiments::ScopedActiveAutofillExperiments() {
  GetActiveExperiments().reset();
}

ScopedActiveAutofillExperiments::~ScopedActiveAutofillExperiments() {
  GetActiveExperiments().reset();
}

AutofillCrowdsourcingManager::QueryResponse::QueryResponse(
    std::string response,
    std::vector<FormSignature> queried_form_signatures)
    : response(std::move(response)),
      queried_form_signatures(std::move(queried_form_signatures)) {}

AutofillCrowdsourcingManager::QueryResponse::QueryResponse(QueryResponse&&) =
    default;
AutofillCrowdsourcingManager::QueryResponse&
AutofillCrowdsourcingManager::QueryResponse::operator=(QueryResponse&&) =
    default;

AutofillCrowdsourcingManager::QueryResponse::~QueryResponse() = default;

AutofillCrowdsourcingManager::AutofillCrowdsourcingManager(AutofillClient* client,
                                                 version_info::Channel channel,
                                                 LogManager* log_manager)
    : AutofillCrowdsourcingManager(client,
                              GetAPIKeyForUrl(channel),
                              log_manager) {}

AutofillCrowdsourcingManager::AutofillCrowdsourcingManager(AutofillClient* client,
                                                 const std::string& api_key,
                                                 LogManager* log_manager)
    : client_(client),
      api_key_(api_key),
      log_manager_(log_manager),
      autofill_server_url_(GetAutofillServerURL()),
      throttle_reset_period_(GetThrottleResetPeriod()),
      max_form_cache_size_(kAutofillCrowdsourcingManagerMaxFormCacheSize),
      loader_backoff_(&kAutofillBackoffPolicy) {}

AutofillCrowdsourcingManager::~AutofillCrowdsourcingManager() = default;

bool AutofillCrowdsourcingManager::IsEnabled() const {
  return autofill_server_url_.is_valid() &&
         base::FeatureList::IsEnabled(
             features::test::kAutofillServerCommunication);
}

bool AutofillCrowdsourcingManager::StartQueryRequest(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
    std::optional<net::IsolationInfo> isolation_info,
    base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
  ScopedCallbackRunner<void(std::optional<QueryResponse>)>
      scoped_callback_runner(std::move(callback), std::nullopt);

  if (!IsEnabled())
    return false;

  // Do not send the request if it contains more fields than the server can
  // accept.
  if (CountActiveFieldsInForms(forms) > kMaxFieldsPerQueryRequest)
    return false;

  // Encode the query for the requested forms.
  auto [query, queried_form_signatures] = EncodeAutofillPageQueryRequest(forms);

  if (queried_form_signatures.empty()) {
    return false;
  }

  // The set of active autofill experiments is constant for the life of the
  // process. We initialize and statically cache it on first use. Leaked on
  // process termination.
  if (!GetActiveExperiments()) {
    InitActiveExperiments();
  }

  // Attach any active autofill experiments.
  query.mutable_experiments()->Reserve(GetActiveExperiments()->size());
  for (variations::VariationID id : *GetActiveExperiments()) {
    query.mutable_experiments()->Add(id);
  }

  std::optional<std::string> payload = GetAPIQueryPayload(query);
  if (!payload) {
    return false;
  }

  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_SENT);

  std::string query_data;
  if (CheckCacheForQueryRequest(queried_form_signatures, &query_data)) {
    LOG_AF(log_manager_) << LoggingScope::kAutofillServer
                         << LogMessage::kCachedAutofillQuery << Br{} << query;
    if (scoped_callback_runner) {
      std::move(scoped_callback_runner)
          .Release()
          .Run(QueryResponse(std::move(query_data),
                             std::move(queried_form_signatures)));
    }
    return true;
  }

  LOG_AF(log_manager_) << LoggingScope::kAutofillServer
                       << LogMessage::kSendAutofillQuery << Br{}
                       << "Signatures: " << query;
  return StartRequest(FormRequestData{
      .callback = std::move(scoped_callback_runner),
      .form_signatures = std::move(queried_form_signatures),
      .request_type = RequestType::kRequestQuery,
      .isolation_info = std::move(isolation_info),
      .payload = std::move(payload).value(),
  });
}

bool AutofillCrowdsourcingManager::StartUploadRequest(
    std::vector<AutofillUploadContents> upload_contents,
    mojom::SubmissionSource form_submission_source,
    bool is_password_manager_upload) {
  if (!IsEnabled()) {
    return false;
  }
  if (upload_contents.empty()) {
    return false;
  }

  PrefService* prefs = client_->GetPrefs();
  const FormSignature form_signature(upload_contents[0].form_signature());
  // Autofill vote uploads are limited via throttling so that only one vote is
  // uploaded per form_submission_source and form signature in a given period of
  // time.
  // Password Manager votes uploaded via specific first occurrences and do not
  // participate in the pref-service tracked throttling mechanism. Always allow
  // Password Manager vote uploads.
  const bool allow_upload =
      is_password_manager_upload ||
      !ShouldThrottleUpload(form_signature, UploadType::kVote,
                            throttle_reset_period_, prefs,
                            form_submission_source) ||
      !base::FeatureList::IsEnabled(features::test::kAutofillUploadThrottling);

  AutofillMetrics::LogUploadEvent(form_submission_source, allow_upload);

  // Metadata throttling does not cancel the upload, but only clears all
  // metadata related entries.
  if (ShouldThrottleUpload(
          form_signature, UploadType::kMetadata, throttle_reset_period_, prefs,
          /*form_submission_source_for_vote_upload=*/std::nullopt)) {
    for (AutofillUploadContents& upload : upload_contents) {
      upload.clear_randomized_form_metadata();
      for (AutofillUploadContents::Field& field :
           *upload.mutable_field_data()) {
        field.clear_randomized_field_metadata();
      }
    }
  }

  // For debugging purposes, even throttled uploads are logged. If no log
  // manager is active, the function can exit early for throttled uploads.
  const bool needs_logging = log_manager_ && log_manager_->IsLoggingActive();
  if (!needs_logging && !allow_upload)
    return false;

  auto Upload = [&](AutofillUploadContents upload) {
    // Get the POST payload that contains upload data.
    std::optional<std::string> payload = GetUploadPayloadForApi(upload);
    if (!payload) {
      return false;
    }

    LOG_AF(log_manager_) << LoggingScope::kAutofillServer
                         << LogMessage::kSendAutofillUpload << Br{}
                         << "Allow upload?: " << allow_upload << Br{}
                         << "Data: " << Br{} << upload;

    if (!allow_upload)
      return false;

    return StartRequest(FormRequestData{
        .form_signatures = {form_signature},
        .request_type = RequestType::kRequestUpload,
        .isolation_info = std::nullopt,
        .payload = std::move(payload).value(),
    });
  };

  bool all_succeeded = true;
  for (AutofillUploadContents& upload : upload_contents) {
    all_succeeded &= Upload(std::move(upload));
  }
  return all_succeeded;
}

void AutofillCrowdsourcingManager::ClearUploadHistory(PrefService* pref_service) {
  if (pref_service) {
    pref_service->ClearPref(prefs::kAutofillVoteUploadEvents);
    pref_service->ClearPref(prefs::kAutofillMetadataUploadEvents);
    pref_service->SetTime(prefs::kAutofillUploadEventsLastResetTimestamp,
                          AutofillClock::Now());
  }
}

size_t AutofillCrowdsourcingManager::GetPayloadLength(
    std::string_view payload) const {
  return payload.length();
}

std::tuple<GURL, std::string> AutofillCrowdsourcingManager::GetRequestURLAndMethod(
    const FormRequestData& request_data) const {
  // ID of the resource to add to the API request URL. Nothing will be added if
  // `resource_id` is empty.
  std::string resource_id;
  std::string method = "POST";

  if (request_data.request_type == RequestType::kRequestQuery) {
    if (GetPayloadLength(request_data.payload) <= kMaxQueryGetSize) {
      resource_id = request_data.payload;
      method = "GET";
      base::UmaHistogramBoolean(kUmaApiUrlIsTooLong, false);
    } else {
      base::UmaHistogramBoolean(kUmaApiUrlIsTooLong, true);
    }
    base::UmaHistogramBoolean(kUmaMethod, method != "GET");
  }

  // Make the canonical URL to query the API, e.g.,
  // https://autofill.googleapis.com/v1/forms/1234?alt=proto.
  GURL url = autofill_server_url_.Resolve(
      GetAPIMethodUrl(request_data.request_type, resource_id, method));

  // Add the query parameter to set the response format to a serialized proto.
  url = net::AppendQueryParameter(url, "alt", "proto");

  return std::make_tuple(std::move(url), std::move(method));
}

bool AutofillCrowdsourcingManager::StartRequest(FormRequestData request_data) {
  // kRequestUploads take no IsolationInfo because Password Manager uploads when
  // RenderFrameHostImpl::DidCommitNavigation() is called, in which case
  // AutofillDriver::IsolationInfo() may crash because there is no committing
  // NavigationRequest. Not setting an IsolationInfo is safe because no
  // information about the response is passed to the renderer, or is otherwise
  // visible to a page. See crbug/1176635#c22.
#if BUILDFLAG(IS_IOS)
  DCHECK(!request_data.isolation_info);
#else
  DCHECK((request_data.request_type == RequestType::kRequestUpload) ==
         !request_data.isolation_info);
#endif
  // Get the URL and method to use for this request.
  auto [request_url, method] = GetRequestURLAndMethod(request_data);

  // Track the URL length for GET queries because the URL length can be in the
  // thousands when rich metadata is enabled.
  if (request_data.request_type == RequestType::kRequestQuery &&
      method == "GET") {
    base::UmaHistogramCounts100000(kUmaGetUrlLength,
                                   request_url.spec().length());
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = method;

  if (request_data.isolation_info) {
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    resource_request->trusted_params->isolation_info =
        *request_data.isolation_info;
  }

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

  // Add API key to the request if a key exists, and the endpoint is trusted by
  // Google.
  if (!api_key_.empty() && request_url.SchemeIs(url::kHttpsScheme) &&
      google_util::IsGoogleAssociatedDomainUrl(request_url)) {
    google_apis::AddAPIKeyToRequest(*resource_request, api_key_);
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
    std::optional<std::string> payload =
        GetAPIBodyPayload(request_data.payload, request_data.request_type);
    if (!payload) {
      return false;
    }
    // Attach payload data and add data format header.
    simple_loader->AttachStringForUpload(std::move(payload).value(),
                                         content_type);
  }

  // Transfer ownership of the loader into url_loaders_. Temporarily hang
  // onto the raw pointer to use it as a key and to kick off the request;
  // transferring ownership (std::move) invalidates the `simple_loader`
  // variable.
  auto* raw_simple_loader = simple_loader.get();
  url_loaders_.push_back(std::move(simple_loader));
  raw_simple_loader->SetTimeoutDuration(kFetchTimeout);
  raw_simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      client_->GetURLLoaderFactory().get(),
      base::BindOnce(&AutofillCrowdsourcingManager::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(--url_loaders_.end()),
                     std::move(request_data), base::TimeTicks::Now()));
  return true;
}

void AutofillCrowdsourcingManager::CacheQueryRequest(
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

bool AutofillCrowdsourcingManager::CheckCacheForQueryRequest(
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
int AutofillCrowdsourcingManager::GetMaxServerAttempts() {
  // This value is constant for the life of the browser, so we cache it
  // statically on first use to avoid re-parsing the param on each retry
  // opportunity.
  static const int max_attempts =
      std::clamp(kAutofillMaxServerAttempts.Get(), 1, 20);
  return max_attempts;
}

void AutofillCrowdsourcingManager::OnSimpleLoaderComplete(
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
      base::TimeTicks::Now() - request_start);

  if (!success) {
    std::string error_message =
        (response_body != nullptr) ? *response_body : "";
    base::UmaHistogramCounts100000(
        GetMetricName(request_data.request_type, "FailingPayloadSize"),
        request_data.payload.length());

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
            base::IgnoreResult(&AutofillCrowdsourcingManager::StartRequest),
            weak_factory_.GetWeakPtr(), std::move(request_data)),
        backoff);
    return;
  }

  if (request_data.request_type != RequestType::kRequestQuery) {
    return;
  }

  CacheQueryRequest(request_data.form_signatures, *response_body);
  base::UmaHistogramBoolean(kUmaWasInCache, simple_loader->LoadedFromCache());
  if (request_data.callback) {
    std::move(request_data.callback)
        .Release()
        .Run(QueryResponse(std::move(*response_body),
                           std::move(request_data.form_signatures)));
  }
}

}  // namespace autofill
