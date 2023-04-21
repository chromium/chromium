// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/device_activity_client.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
// TODO(https://crbug.com/1269900): Migrate to use SFUL library.
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_sync_observation.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Amount of time to wait before retriggering repeating timer.
constexpr base::TimeDelta kTimeToRepeat = base::Hours(1);

// Milliseconds per minute.
constexpr int kMillisecondsPerMinute = 60000;

// General upper bound of expected Fresnel response size in bytes.
constexpr size_t kMaxFresnelResponseSizeBytes = 5 << 20;  // 5MB;

// Maximum time to wait for time sync before not reporting device as active
// in current attempt.
// This corresponds to at least seven TCP retransmissions attempts to
// the remote server used to update the system clock.
constexpr base::TimeDelta kSystemClockSyncWaitTimeout = base::Seconds(45);

// Timeout for each Fresnel request.
constexpr base::TimeDelta kHealthCheckRequestTimeout = base::Seconds(10);
constexpr base::TimeDelta kImportRequestTimeout = base::Seconds(15);
constexpr base::TimeDelta kOprfRequestTimeout = base::Seconds(15);
constexpr base::TimeDelta kQueryRequestTimeout = base::Seconds(65);

// TODO(https://crbug.com/1272922): Move shared configuration constants to
// separate file.
const char kFresnelHealthCheckEndpoint[] = "/v1/fresnel/healthCheck";
const char kFresnelImportRequestEndpoint[] = "/v1/fresnel/psmRlweImport";
const char kFresnelOprfRequestEndpoint[] = "/v1/fresnel/psmRlweOprf";
const char kFresnelQueryRequestEndpoint[] = "/v1/fresnel/psmRlweQuery";

// UMA histograms defined in:
// //tools/metrics/histograms/metadata/ash/histograms.xml.
//
// Count number of times a state has been entered.
const char kHistogramStateCount[] = "Ash.DeviceActiveClient.StateCount";

// Record the preserved file state.
const char kHistogramsPreservedFileState[] =
    "Ash.DeviceActiveClient.PreservedFileState";

// Record the Check Membership process cases.
const char kCheckMembershipProcessCase[] =
    "Ash.DeviceActiveClient.CheckMembershipCases";

// Duration histogram uses State variant in order to create
// unique histograms measuring durations by State.
const char kHistogramDurationPrefix[] = "Ash.DeviceActiveClient.Duration";

// Response histogram uses State variant in order to create
// unique histograms measuring responses by State.
const char kHistogramResponsePrefix[] = "Ash.DeviceActiveClient.Response";

// Count the number of boolean membership request results.
const char kDeviceActiveClientQueryMembershipResult[] =
    "Ash.DeviceActiveClient.QueryMembershipResult";

// Record number of successful saves of the preserved file content.
const char kDeviceActiveClientSavePreservedFileSuccess[] =
    "Ash.DeviceActiveClient.SavePreservedFileSuccess";

// Record the minute the device activity client transitions out of idle.
const char kDeviceActiveClientTransitionOutOfIdleMinute[] =
    "Ash.DeviceActiveClient.RecordedTransitionOutOfIdleMinute";

// Record the minute the device activity client transitions to check in.
const char kDeviceActiveClientTransitionToCheckInMinute[] =
    "Ash.DeviceActiveClient.RecordedTransitionToCheckInMinute";

// Record the NetError status integer returned by the OPRF network response.
const char kDeviceActiveClientPsmOprfResponseNetErrorCode[] =
    "Ash.DeviceActiveClient.PsmOprfResponseNetErrorCode";

// Record a boolean success if the PSM Oprf response body exists.
const char kDeviceActiveClientIsPsmOprfResponseBodySet[] =
    "Ash.DeviceActiveClient.IsPsmOprfResponseBodySet";

// Record a boolean success if the PSM Oprf response body was parsed correctly
// to the FresnelPsmRlweOprfResponse proto object.
const char kDeviceActiveClientIsPsmOprfResponseParsedCorrectly[] =
    "Ash.DeviceActiveClient.IsPsmOprfResponseParsedCorrectly";

// Record the NetError status integer returned by the Query network response.
const char kDeviceActiveClientPsmQueryResponseNetErrorCode[] =
    "Ash.DeviceActiveClient.PsmQueryResponseNetErrorCode";

// Record a boolean success if the PSM Query response body exists.
const char kDeviceActiveClientIsPsmQueryResponseBodySet[] =
    "Ash.DeviceActiveClient.IsPsmQueryResponseBodySet";

// Record a boolean success if the PSM Query response body was parsed correctly
// to the FresnelPsmRlweQueryResponse proto object.
const char kDeviceActiveClientIsPsmQueryResponseParsedCorrectly[] =
    "Ash.DeviceActiveClient.IsPsmQueryResponseParsedCorrectly";

// Traffic annotation for check device activity status
const net::NetworkTrafficAnnotationTag check_membership_traffic_annotation =
    net::DefineNetworkTrafficAnnotation(
        "device_activity_client_check_membership",
        R"(
        semantics {
          sender: "Device Activity"
          description:
            "Check the status of the Chrome OS devices in a private "
            "set, through Private Set Membership (PSM) services."
          trigger: "Chrome OS client makes this network request and records "
                   "the device activity when the default network changes"
          data: "Google API Key."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Not implemented."
        })");

// Returns the total offset between Pacific Time (PT) and GMT.
// Parameter ts is expected to be GMT/UTC.
// TODO(hirthanan): Create utils library for commonly used methods.
base::Time ConvertToPT(base::Time ts) {
  // America/Los_Angleles is PT.
  std::unique_ptr<icu::TimeZone> time_zone(
      icu::TimeZone::createTimeZone("America/Los_Angeles"));
  if (*time_zone == icu::TimeZone::getUnknown()) {
    LOG(ERROR) << "Failed to get America/Los_Angeles timezone. "
               << "Returning UTC-8 timezone as default.";
    return ts - base::Hours(8);
  }

  // Calculate timedelta between PT and GMT. This method does not take day light
  // savings (DST) into account.
  const base::TimeDelta raw_time_diff =
      base::Minutes(time_zone->getRawOffset() / kMillisecondsPerMinute);

  UErrorCode status = U_ZERO_ERROR;
  auto gregorian_calendar =
      std::make_unique<icu::GregorianCalendar>(*time_zone, status);

  // Calculates the time difference adjust by the possible daylight savings
  // offset. If the status of any step fails, returns the default time
  // difference without considering daylight savings.
  if (!gregorian_calendar) {
    return ts + raw_time_diff;
  }

  // Convert ts object to UDate.
  UDate current_date =
      static_cast<UDate>(ts.ToDoubleT() * base::Time::kMillisecondsPerSecond);
  status = U_ZERO_ERROR;
  gregorian_calendar->setTime(current_date, status);
  if (U_FAILURE(status)) {
    return ts + raw_time_diff;
  }

  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar->inDaylightTime(status);
  if (U_FAILURE(status)) {
    return ts + raw_time_diff;
  }

  // Calculate timedelta between PT and GMT, taking DST into account for an
  // accurate PT.
  int gmt_offset = time_zone->getRawOffset();
  if (day_light) {
    gmt_offset += time_zone->getDSTSavings();
  }

  return ts + base::Minutes(gmt_offset / kMillisecondsPerMinute);
}

base::Time GetNextMonth(base::Time ts) {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  // Set new time to the first midnight of the next month.
  exploded.day_of_month = 1;
  exploded.month += 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  // Handle case when month is December.
  if (exploded.month > 12) {
    exploded.year += 1;
    exploded.month = 1;
  }

  base::Time new_month_ts;
  bool success = base::Time::FromUTCExploded(exploded, &new_month_ts);

  if (!success) {
    return base::Time();
  }

  return new_month_ts;
}

// Generates the full histogram name for histogram variants based on state.
std::string HistogramVariantName(const std::string& histogram_prefix,
                                 DeviceActivityClient::State state) {
  switch (state) {
    case DeviceActivityClient::State::kIdle:
      return base::StrCat({histogram_prefix, ".Idle"});
    case DeviceActivityClient::State::kCheckingMembershipOprf:
      return base::StrCat({histogram_prefix, ".CheckingMembershipOprf"});
    case DeviceActivityClient::State::kCheckingMembershipQuery:
      return base::StrCat({histogram_prefix, ".CheckingMembershipQuery"});
    case DeviceActivityClient::State::kCheckingIn:
      return base::StrCat({histogram_prefix, ".CheckingIn"});
    case DeviceActivityClient::State::kHealthCheck:
      return base::StrCat({histogram_prefix, ".HealthCheck"});
    default:
      NOTREACHED() << "Invalid State.";
      return base::StrCat({histogram_prefix, ".Unknown"});
  }
}

void RecordStateCountMetric(DeviceActivityClient::State state) {
  base::UmaHistogramEnumeration(kHistogramStateCount, state);
}

void RecordQueryMembershipResultBoolean(bool is_member) {
  base::UmaHistogramBoolean(kDeviceActiveClientQueryMembershipResult,
                            is_member);
}

void RecordSavePreservedFile(bool success) {
  base::UmaHistogramBoolean(kDeviceActiveClientSavePreservedFileSuccess,
                            success);
}

// Return the minute of the current PT time.
int GetCurrentMinute() {
  base::Time cur_time = ConvertToPT(base::Time::Now());

  // Extract minute from exploded |cur_time|.
  base::Time::Exploded exploded_utc;
  cur_time.UTCExplode(&exploded_utc);

  return exploded_utc.minute;
}

void RecordTransitionOutOfIdleMinute() {
  base::UmaHistogramCustomCounts(kDeviceActiveClientTransitionOutOfIdleMinute,
                                 GetCurrentMinute(), 0, 59,
                                 60 /* number of histogram buckets */);
}

void RecordTransitionToCheckInMinute() {
  base::UmaHistogramCustomCounts(kDeviceActiveClientTransitionToCheckInMinute,
                                 GetCurrentMinute(), 0, 59,
                                 60 /* number of histogram buckets */);
}

// Histogram sliced by duration and state.
void RecordDurationStateMetric(DeviceActivityClient::State state,
                               const base::TimeDelta duration) {
  std::string duration_state_histogram_name =
      HistogramVariantName(kHistogramDurationPrefix, state);
  base::UmaHistogramCustomTimes(duration_state_histogram_name, duration,
                                base::Milliseconds(1), base::Seconds(100),
                                100 /* number of histogram buckets */);
}

// Histogram slices by PSM response and state.
void RecordResponseStateMetric(DeviceActivityClient::State state,
                               int net_code) {
  // Mapping status code to PsmResponse is used to record UMA histograms
  // for responses by state.
  DeviceActivityClient::PsmResponse response;
  switch (net_code) {
    case net::OK:
      response = DeviceActivityClient::PsmResponse::kSuccess;
      break;
    case net::ERR_TIMED_OUT:
      response = DeviceActivityClient::PsmResponse::kTimeout;
      break;
    default:
      response = DeviceActivityClient::PsmResponse::kError;
      break;
  }

  base::UmaHistogramEnumeration(
      HistogramVariantName(kHistogramResponsePrefix, state), response);
}

// Histogram to record number of each PreservedFileState.
void RecordPreservedFileState(
    DeviceActivityClient::PreservedFileState preserved_file_state) {
  base::UmaHistogramEnumeration(kHistogramsPreservedFileState,
                                preserved_file_state);
}

// Histogram to record number of different failed/success cases for check
// membership process.
void RecordCheckMembershipCases(
    DeviceActivityClient::CheckMembershipResponseCases check_membership_case) {
  base::UmaHistogramEnumeration(kCheckMembershipProcessCase,
                                check_membership_case);
}

// Histogram to record the NetError code returned apart of the PSM Oprf
// Response.
void RecordPsmOprfResponseNetErrorCode(int net_error) {
  base::UmaHistogramSparse(kDeviceActiveClientPsmOprfResponseNetErrorCode,
                           net_error);
}

// Histogram to record whether the PSM Oprf response body is set.
void RecordIsPsmOprfResponseBodySet(bool is_set) {
  base::UmaHistogramBoolean(kDeviceActiveClientIsPsmOprfResponseBodySet,
                            is_set);
}

// Histogram to record whether the PSM Oprf response was able to be parsed
// correctly.
void RecordIsPsmOprfResponseParsedCorrectly(bool is_parsed_correctly) {
  base::UmaHistogramBoolean(kDeviceActiveClientIsPsmOprfResponseParsedCorrectly,
                            is_parsed_correctly);
}

// Histogram to record the NetError code returned apart of the PSM Query
// Response.
void RecordPsmQueryResponseNetErrorCode(int net_error) {
  base::UmaHistogramSparse(kDeviceActiveClientPsmQueryResponseNetErrorCode,
                           net_error);
}

// Histogram to record whether the PSM Query response body is set.
void RecordIsPsmQueryResponseBodySet(bool is_set) {
  base::UmaHistogramBoolean(kDeviceActiveClientIsPsmQueryResponseBodySet,
                            is_set);
}

// Histogram to record whether the PSM Query response was able to be parsed
// correctly.
void RecordIsPsmQueryResponseParsedCorrectly(bool is_parsed_correctly) {
  base::UmaHistogramBoolean(
      kDeviceActiveClientIsPsmQueryResponseParsedCorrectly,
      is_parsed_correctly);
}

std::unique_ptr<network::ResourceRequest> GenerateResourceRequest(
    const std::string& request_method,
    const GURL& url,
    const std::string& api_key) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = request_method;
  resource_request->headers.SetHeader("x-goog-api-key", api_key);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/x-protobuf");

  return resource_request;
}

}  // namespace

// static
void DeviceActivityClient::RecordDeviceActivityMethodCalled(
    DeviceActivityMethod method_name) {
  // Record the device activity method calls.
  const char kDeviceActivityMethodCalled[] = "Ash.DeviceActivity.MethodCalled";

  base::UmaHistogramEnumeration(kDeviceActivityMethodCalled, method_name);
}

DeviceActivityClient::DeviceActivityClient(
    ChurnActiveStatus* churn_active_status_ptr,
    PrefService* local_state,
    NetworkStateHandler* handler,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<base::RepeatingTimer> report_timer,
    const std::string& fresnel_base_url,
    const std::string& api_key,
    base::Time chrome_first_run_time,
    std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases)
    : churn_active_status_ptr_(churn_active_status_ptr),
      local_state_(local_state),
      network_state_handler_(handler),
      url_loader_factory_(url_loader_factory),
      report_timer_(std::move(report_timer)),
      fresnel_base_url_(fresnel_base_url),
      api_key_(api_key),
      chrome_first_run_time_(chrome_first_run_time),
      use_cases_(std::move(use_cases)) {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientConstructor);

  DCHECK(network_state_handler_);
  DCHECK(url_loader_factory_);
  DCHECK(report_timer_);
  DCHECK(!use_cases_.empty());

  report_timer_->Start(FROM_HERE, kTimeToRepeat, this,
                       &DeviceActivityClient::ReportingTriggeredByTimer);

  network_state_handler_observer_.Observe(network_state_handler_.get());

  // Send DBus method to read preserved files for last ping timestamps.
  GetLastPingDatesStatus();
}

DeviceActivityClient::~DeviceActivityClient() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientDestructor);
}

base::RepeatingTimer* DeviceActivityClient::GetReportTimer() {
  return report_timer_.get();
}

// Method gets called when the state of the default (primary)
// network OR properties of the default network changes.
void DeviceActivityClient::DefaultNetworkChanged(const NetworkState* network) {
  bool was_connected = network_connected_;
  network_connected_ = network && network->IsOnline();

  if (network_connected_ == was_connected)
    return;

  if (network_connected_)
    OnNetworkOnline();
  else
    OnNetworkOffline();
}

void DeviceActivityClient::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

DeviceActivityClient::State DeviceActivityClient::GetState() const {
  return state_;
}

std::vector<DeviceActiveUseCase*> DeviceActivityClient::GetUseCases() const {
  std::vector<DeviceActiveUseCase*> use_cases_ptr;

  for (auto& use_case : use_cases_) {
    use_cases_ptr.push_back(use_case.get());
  }
  return use_cases_ptr;
}

DeviceActiveUseCase* DeviceActivityClient::GetUseCasePtr(
    psm_rlwe::RlweUseCase psm_use_case) const {
  for (auto* use_case : GetUseCases()) {
    if (use_case->GetPsmUseCase() == psm_use_case) {
      return use_case;
    }
  }

  VLOG(1) << "Use Case is not supported yet.";
  return nullptr;
}

void DeviceActivityClient::UpdateChurnLocalStateAfterCheckIn(
    DeviceActiveUseCase* current_use_case) {
  if (current_use_case->GetPsmUseCase() ==
      private_membership::rlwe::RlweUseCase::
          CROS_FRESNEL_CHURN_MONTHLY_COHORT) {
    base::Time prev_active_month =
        churn_active_status_ptr_->GetCurrentActiveMonth();

    // Update the active value based on the new ping timestamp.
    // This will also require updating the booleans in local state that store
    // whether a device has pinged in the past 3 months relative to the current
    // month.
    churn_active_status_ptr_->UpdateValue(last_transition_out_of_idle_time_);
    base::Time new_active_month =
        churn_active_status_ptr_->GetCurrentActiveMonth();
    int new_active_value = churn_active_status_ptr_->GetValueAsInt();

    // No updates were made to the churn active status.
    if (prev_active_month == new_active_month) {
      return;
    }

    // Read the current relative observation period reported status.
    bool is_active_current_period_minus_0 = local_state_->GetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0);
    bool is_active_current_period_minus_1 = local_state_->GetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1);
    bool is_active_current_period_minus_2 = local_state_->GetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2);

    // Determine how many months have passed between new_active_value and
    // prev_active_value. This will tell us how many months the relative
    // observation status should shift by in the local state.
    for (int i = 0; i < 3; i++) {
      if (prev_active_month == new_active_month) {
        break;
      }

      // Shift relative observation windows reported status by 1.
      // This is for the cohort use case, which happens before the observation
      // use case. This means that after a successful churn cohort ping in a
      // new month, the device may have false observation report statuses.
      is_active_current_period_minus_2 = is_active_current_period_minus_1;
      is_active_current_period_minus_1 = is_active_current_period_minus_0;
      is_active_current_period_minus_0 = false;

      prev_active_month = GetNextMonth(prev_active_month);
    }

    // Update the local state values after determining all other values.
    local_state_->SetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0,
        is_active_current_period_minus_0);
    local_state_->SetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1,
        is_active_current_period_minus_1);
    local_state_->SetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2,
        is_active_current_period_minus_2);
    local_state_->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                             new_active_value);
  }

  // Update the relative local state observation period boolean if the
  // observation use case had set a value for the observation period.
  if (current_use_case->GetPsmUseCase() ==
      private_membership::rlwe::RlweUseCase::
          CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION) {
    if (!current_use_case->GetObservationPeriod(0).empty()) {
      local_state_->SetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, true);
    }

    if (!current_use_case->GetObservationPeriod(1).empty()) {
      local_state_->SetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
    }

    if (!current_use_case->GetObservationPeriod(2).empty()) {
      local_state_->SetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, true);
    }
  }
}

void DeviceActivityClient::ReadChurnPreservedFile(
    const private_computing::ActiveStatus& status) {
  private_computing::PrivateComputingUseCase use_case = status.use_case();

  if (use_case == private_computing::PrivateComputingUseCase::
                      CROS_FRESNEL_CHURN_MONTHLY_COHORT) {
    int churn_status_value = 0;
    if (status.has_churn_active_status()) {
      churn_status_value = status.churn_active_status();
    }
    if (local_state_->GetInteger(
            prefs::kDeviceActiveLastKnownChurnActiveStatus) == 0 &&
        churn_status_value != 0) {
      churn_active_status_ptr_->SetValue(churn_status_value);
      local_state_->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                               churn_status_value);
    }
  }

  if (use_case == private_computing::PrivateComputingUseCase::
                      CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION) {
    bool is_active_current_period_minus_0 = false;
    bool is_active_current_period_minus_1 = false;
    bool is_active_current_period_minus_2 = false;

    // Get preserved file period status values.
    if (status.has_period_status()) {
      private_computing::ChurnObservationStatus period_status =
          status.period_status();
      is_active_current_period_minus_0 =
          period_status.is_active_current_period_minus_0();
      is_active_current_period_minus_1 =
          period_status.is_active_current_period_minus_1();
      is_active_current_period_minus_2 =
          period_status.is_active_current_period_minus_2();
    }

    // Try to update the local state using the preserved file values.
    // The preserved file may update the period status to true if the device
    // sent the observation window previously and then was powerwashed.
    // The local state will not currently be updated on recovery since that data
    // needs to be recovered using check membership requests in the future.
    if (!local_state_->GetBoolean(
            prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0)) {
      local_state_->SetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0,
          is_active_current_period_minus_0);
    }
    if (!local_state_->GetBoolean(
            prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1)) {
      local_state_->SetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1,
          is_active_current_period_minus_1);
    }
    if (!local_state_->GetBoolean(
            prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2)) {
      local_state_->SetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2,
          is_active_current_period_minus_2);
    }

    // Update churn observation last ping timestamp in local state based on
    // active status value. The cohort use case is recovered before the
    // observation use case, so we use the latest active status value.
    local_state_->SetTime(
        prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp,
        churn_active_status_ptr_->GetCurrentActiveMonth());
  }
}

private_computing::SaveStatusRequest
DeviceActivityClient::GetSaveStatusRequest() {
  // private_computing:
  private_computing::SaveStatusRequest request;

  for (auto* use_case : GetUseCases()) {
    private_computing::ActiveStatus status = use_case->GenerateActiveStatus();
    if (status.has_use_case()) {
      *request.add_active_status() = status;
    }
  }

  return request;
}

void DeviceActivityClient::SaveLastPingDatesStatus() {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientSaveLastPingDatesStatus);
  private_computing::SaveStatusRequest request = GetSaveStatusRequest();

  // Call DBus method with callback to |OnSaveLastPingDatesStatusComplete|.
  PrivateComputingClient::Get()->SaveLastPingDatesStatus(
      request,
      base::BindOnce(&DeviceActivityClient::OnSaveLastPingDatesStatusComplete,
                     weak_factory_.GetWeakPtr()));
}

void DeviceActivityClient::OnSaveLastPingDatesStatusComplete(
    private_computing::SaveStatusResponse response) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnSaveLastPingDatesStatusComplete);
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to store last ping timestamps with error message: "
            << response.error_message();
    RecordSavePreservedFile(false);
  } else {
    VLOG(1) << "Successfully stored last ping timestamp to preserved file";
    RecordSavePreservedFile(true);
  }
}

void DeviceActivityClient::GetLastPingDatesStatus() {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientGetLastPingDatesStatus);
  PrivateComputingClient::Get()->GetLastPingDatesStatus(
      base::BindOnce(&DeviceActivityClient::OnGetLastPingDatesStatusFetched,
                     weak_factory_.GetWeakPtr()));
}

void DeviceActivityClient::OnGetLastPingDatesStatusFetched(
    private_computing::GetStatusResponse response) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnGetLastPingDatesStatusFetched);
  // Update the last ping timestamps if the preserved file has a valid
  // timestamp value for the use case.
  if (!response.has_error_message()) {
    VLOG(1) << "Successfully read PSM file.";
    // 1. Iterate FileContent for the active_statuses and update use case
    // timestamps.
    for (auto& status : response.active_status()) {
      // One of last_ping_date and period_status is set in ActiveStatus proto.
      base::Time last_ping_time;
      if (status.has_last_ping_date()) {
        std::string last_ping_pt_date = status.last_ping_date();

        bool success = base::Time::FromUTCString(last_ping_pt_date.c_str(),
                                                 &last_ping_time);

        if (!success) {
          continue;
        }
      }

      // TODO(hirthanan): Get/Set period status for churn observation status in
      // future CL.
      private_computing::ChurnObservationStatus period_status;
      if (status.has_period_status()) {
        period_status = status.period_status();
      }

      DeviceActiveUseCase* device_active_use_case_ptr;

      private_computing::PrivateComputingUseCase use_case = status.use_case();
      switch (use_case) {
        case private_computing::PrivateComputingUseCase::CROS_FRESNEL_DAILY:
          device_active_use_case_ptr =
              GetUseCasePtr(psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY);
          break;
        case private_computing::PrivateComputingUseCase::
            CROS_FRESNEL_28DAY_ACTIVE:
          device_active_use_case_ptr =
              GetUseCasePtr(psm_rlwe::RlweUseCase::CROS_FRESNEL_28DAY_ACTIVE);
          break;
        case private_computing::PrivateComputingUseCase::
            CROS_FRESNEL_CHURN_MONTHLY_COHORT:
          device_active_use_case_ptr = GetUseCasePtr(
              psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT);
          break;
        case private_computing::PrivateComputingUseCase::
            CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION:
          device_active_use_case_ptr = GetUseCasePtr(
              psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION);
          break;
        default:
          LOG(ERROR) << "PSM use case is not supported yet.";
          continue;
      }

      // Crashes may occur due to device_active_use_case_ptr not being defined
      // at this point.
      if (device_active_use_case_ptr == nullptr) {
        LOG(ERROR) << "Device active use case is not defined.";
        return;
      }

      // Recover the churn active value and ChurnActiveStatus
      // if set in preserved file but not in local state.
      ReadChurnPreservedFile(status);

      if (!device_active_use_case_ptr->IsLastKnownPingTimestampSet()) {
        if (use_case ==
            private_computing::PrivateComputingUseCase::CROS_FRESNEL_DAILY) {
          // Only record the successfully read from preserved file for daily use
          // case.
          RecordPreservedFileState(
              DeviceActivityClient::PreservedFileState::kReadOkLocalStateEmpty);
        }
        VLOG(1) << "Updating local pref timestamp value with file timestamp = "
                << last_ping_time;
        device_active_use_case_ptr->SetLastKnownPingTimestamp(last_ping_time);
      } else {
        if (use_case ==
            private_computing::PrivateComputingUseCase::CROS_FRESNEL_DAILY) {
          // Only record the successfully read from preserved file for daily use
          // case.
          RecordPreservedFileState(
              DeviceActivityClient::PreservedFileState::kReadOkLocalStateSet);
        }
        VLOG(1) << "Preserved File was read successfully but local state is "
                   "already set. "
                << "Device was most likely restarted and not powerwashed, so "
                   "no need to update local state.";
      }
    }
  } else {
    base::Time current_time = base::Time::Now();
    // If the device is not a new device and the local pref is empty, then
    // record the error count in uma histogram.
    if ((current_time - chrome_first_run_time_) > base::Days(1)) {
      DeviceActiveUseCase* device_active_use_case_ptr =
          GetUseCasePtr(psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY);
      if (!device_active_use_case_ptr->IsLastKnownPingTimestampSet()) {
        // Local pref is empty. To avoid when the chrome signout or reboot
        // to record unnecessary uma hisgtogram.
        RecordPreservedFileState(
            DeviceActivityClient::PreservedFileState::kReadFail);
        LOG(ERROR)
            << "Preserved File read has failed. State of local states is "
               "not checked. "
            << "Error from DBus: " << response.error_message();
      }
    }
  }

  // Always trigger step to check for network status changing after reading the
  // preserved file.
  DefaultNetworkChanged(network_state_handler_->DefaultNetwork());
}

void DeviceActivityClient::ReportingTriggeredByTimer() {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientReportingTriggeredByTimer);

  // Terminate if the state of the client while reporting device actives is not
  // what is expected. This may occur if the client is in the middle of
  // reporting actives or is disconnected from the network.
  if (!network_connected_ || state_ != State::kIdle ||
      !pending_use_cases_.empty()) {
    TransitionToIdle(nullptr);
    return;
  }

  OnNetworkOnline();
}

void DeviceActivityClient::OnNetworkOnline() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientOnNetworkOnline);

  // Asynchronously wait for the system clock to synchronize on network
  // connection.
  system_clock_sync_observation_ =
      SystemClockSyncObservation::WaitForSystemClockSync(
          SystemClockClient::Get(), kSystemClockSyncWaitTimeout,
          base::BindOnce(&DeviceActivityClient::OnSystemClockSyncResult,
                         weak_factory_.GetWeakPtr()));
}

void DeviceActivityClient::OnSystemClockSyncResult(
    bool system_clock_synchronized) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnSystemClockSyncResult);

  if (system_clock_synchronized)
    ReportUseCases();
  else
    TransitionToIdle(nullptr);
}

void DeviceActivityClient::OnNetworkOffline() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientOnNetworkOffline);
  CancelUseCases();
}

GURL DeviceActivityClient::GetFresnelURL() const {
  GURL base_url(fresnel_base_url_);
  GURL::Replacements replacements;

  switch (state_) {
    case State::kCheckingMembershipOprf:
      replacements.SetPathStr(kFresnelOprfRequestEndpoint);
      break;
    case State::kCheckingMembershipQuery:
      replacements.SetPathStr(kFresnelQueryRequestEndpoint);
      break;
    case State::kCheckingIn:
      replacements.SetPathStr(kFresnelImportRequestEndpoint);
      break;
    case State::kHealthCheck:
      replacements.SetPathStr(kFresnelHealthCheckEndpoint);
      break;
    case State::kIdle:  // Fallthrough to |kUnknown| case.
      [[fallthrough]];
    case State::kUnknown:
      NOTREACHED();
      break;
  }

  return base_url.ReplaceComponents(replacements);
}

// TODO(https://crbug.com/1262189): Add callback to report actives only after
// synchronizing the system clock.
void DeviceActivityClient::ReportUseCases() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientReportUseCases);

  DCHECK(!use_cases_.empty());

  if (!network_connected_ || state_ != State::kIdle) {
    TransitionToIdle(nullptr);
    return;
  }

  // Adjust UTC time object to represent PT for entire device activity client.
  last_transition_out_of_idle_time_ = ConvertToPT(base::Time::Now());

  for (auto& use_case : use_cases_) {
    // Ownership of the use cases will be maintained by the |use_cases_| vector.
    pending_use_cases_.push(use_case.get());
  }

  // Pop from |pending_use_cases_| queue in |TransitionToIdle|, after the
  // use case has tried to be reported.
  TransitionOutOfIdle(pending_use_cases_.front());
}

void DeviceActivityClient::CancelUseCases() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientCancelUseCases);

  // Use RAII to reset |url_loader_| after current function scope.
  // Delete |url_loader_| before the callback is invoked cancels the sent out
  // request.
  // No callback will be invoked in the case a network request is sent,
  // and the device internet disconnects.
  auto url_loader = std::move(url_loader_);

  // Use RAII to clear the queue.
  std::queue<DeviceActiveUseCase*> pending_use_cases;
  std::swap(pending_use_cases_, pending_use_cases);

  for (auto* use_case : GetUseCases()) {
    use_case->ClearSavedState();
  }

  TransitionToIdle(nullptr);
}

void DeviceActivityClient::TransitionOutOfIdle(
    DeviceActiveUseCase* current_use_case) {
  RecordTransitionOutOfIdleMinute();
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionOutOfIdle);

  DCHECK(current_use_case);

  // Begin phase one of checking membership if the device has not pinged yet
  // within the given use case window.
  // TODO(https://crbug.com/1262187): Remove hardcoded use case when adding
  // support for additional use cases.
  if (current_use_case->IsDevicePingRequired(
          last_transition_out_of_idle_time_)) {
    bool success = current_use_case->SetWindowIdentifier(
        last_transition_out_of_idle_time_);

    if (!success) {
      TransitionToIdle(current_use_case);
      return;
    }

    // Check membership continues when the cached local state pref
    // is not set. The local state pref may not be set if the device is
    // new, powerwashed, recovered, RMA, or the local state was corrupted.
    if (current_use_case->IsEnabledCheckMembership() &&
        !current_use_case->IsLastKnownPingTimestampSet()) {
      TransitionToCheckMembershipOprf(current_use_case);
      return;
    }

    // |TransitionToCheckIn| if the local state pref is set.
    // During rollout, we perform CheckIn without CheckMembership for
    // powerwash, recovery, or RMA devices.
    if (current_use_case->IsEnabledCheckIn()) {
      TransitionToCheckIn(current_use_case);
      return;
    }
  }

  TransitionToIdle(current_use_case);
}

void DeviceActivityClient::TransitionToHealthCheck() {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionToHealthCheck);

  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!url_loader_);

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("device_activity_client_health_check",
                                          R"(
        semantics {
          sender: "Device Activity Health Check"
          description:
            "Send Health Check network request of Chrome OS device. "
            "Provide a health check service for client. "
            "The health check will include cpu utilization, "
            "memory usage and disk space. "
            "The server will return health status of the service immediately. "
            "The health status will include if the device is actively running "
            "or the device is not successfully sending heartbeats to servers "
            "or the device is not eligible for health monitoring. "
          trigger: "This request is deprecated, and never happens."
          data: "Google API Key."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Not implemented."
        })");

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kHealthCheck;

  // Report UMA histogram for transitioning state to |kHealthCheck|.
  RecordStateCountMetric(state_);

  auto resource_request = GenerateResourceRequest(
      net::HttpRequestHeaders::kGetMethod, GetFresnelURL(), api_key_);

  // TODO(https://crbug.com/1266972): Refactor |url_loader_| network request
  // call to a shared helper method.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->SetTimeoutDuration(kHealthCheckRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceActivityClient::OnHealthCheckDone,
                     weak_factory_.GetWeakPtr()),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnHealthCheckDone(
    std::unique_ptr<std::string> response_body) {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientOnHealthCheckDone);

  DCHECK_EQ(state_, State::kHealthCheck);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);

  // Record duration of |kHealthCheck| state.
  RecordDurationStateMetric(state_, state_timer_.Elapsed());

  // Transition back to kIdle state after performing a health check on servers.
  TransitionToIdle(nullptr);
}

void DeviceActivityClient::TransitionToCheckMembershipOprf(
    DeviceActiveUseCase* current_use_case) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionToCheckMembershipOprf);

  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingMembershipOprf;

  // Report UMA histogram for transitioning state to |kCheckingMembershipOprf|.
  RecordStateCountMetric(state_);

  std::vector<psm_rlwe::RlwePlaintextId> psm_ids =
      current_use_case->GetPsmIdentifiersToQuery();

  // Initializes the PSM rlwe client with the appropriate psm id values that we
  // want to check membership for. This varies by fixed and n-day use cases.
  current_use_case->SetPsmRlweClient(psm_ids);

  // Generate PSM Oprf request body.
  const auto status_or_oprf_request =
      current_use_case->GetPsmRlweClient()->CreateOprfRequest();
  if (!status_or_oprf_request.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::
            kOprfResponseBodyFailed);
    TransitionToIdle(current_use_case);
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfRequest oprf_request =
      status_or_oprf_request.value();

  // Wrap PSM Oprf request body by FresnelPsmRlweOprfRequest proto.
  // This proto is expected by the Fresnel service.
  device_activity::FresnelPsmRlweOprfRequest fresnel_oprf_request;
  *fresnel_oprf_request.mutable_rlwe_oprf_request() = oprf_request;

  std::string request_body;
  fresnel_oprf_request.SerializeToString(&request_body);

  auto resource_request = GenerateResourceRequest(
      net::HttpRequestHeaders::kPostMethod, GetFresnelURL(), api_key_);

  // TODO(https://crbug.com/1266972): Refactor |url_loader_| network request
  // call to a shared helper method.
  url_loader_ = network ::SimpleURLLoader ::Create(
      std::move(resource_request), check_membership_traffic_annotation);
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(kOprfRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceActivityClient::OnCheckMembershipOprfDone,
                     weak_factory_.GetWeakPtr(), current_use_case),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckMembershipOprfDone(
    DeviceActiveUseCase* current_use_case,
    std::unique_ptr<std::string> response_body) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnCheckMembershipOprfDone);

  DCHECK_EQ(state_, State::kCheckingMembershipOprf);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);
  RecordPsmOprfResponseNetErrorCode(net_code);

  // Convert serialized response body to oprf response protobuf.
  // Add UMA histogram for diagnostic purposes.
  FresnelPsmRlweOprfResponse psm_oprf_response;
  bool is_response_body_set = response_body.get() != nullptr;
  RecordIsPsmOprfResponseBodySet(is_response_body_set);

  if (!is_response_body_set ||
      !psm_oprf_response.ParseFromString(*response_body)) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::
            kOprfResponseBodyFailed);
    RecordIsPsmOprfResponseParsedCorrectly(false);

    TransitionToIdle(current_use_case);
    return;
  }

  // Oprf response was parsed successfully.
  RecordIsPsmOprfResponseParsedCorrectly(true);

  // Parse |fresnel_oprf_response| for oprf_response.
  if (!psm_oprf_response.has_rlwe_oprf_response()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::
            kNotHasRlweOprfResponse);
    TransitionToIdle(current_use_case);
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
      psm_oprf_response.rlwe_oprf_response();

  RecordDurationStateMetric(state_, state_timer_.Elapsed());
  TransitionToCheckMembershipQuery(oprf_response, current_use_case);
}

void DeviceActivityClient::TransitionToCheckMembershipQuery(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response,
    DeviceActiveUseCase* current_use_case) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionToCheckMembershipQuery);

  DCHECK_EQ(state_, State::kCheckingMembershipOprf);
  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingMembershipQuery;

  // Report UMA histogram for transitioning state to |kCheckingMembershipQuery|.
  RecordStateCountMetric(state_);

  // Generate PSM Query request body.
  const auto status_or_query_request =
      current_use_case->GetPsmRlweClient()->CreateQueryRequest(oprf_response);
  if (!status_or_query_request.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::
            kCreateQueryRequestFailed);
    TransitionToIdle(current_use_case);
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryRequest query_request =
      status_or_query_request.value();

  // Wrap PSM Query request body by FresnelPsmRlweQueryRequest proto.
  // This proto is expected by the Fresnel service.
  device_activity::FresnelPsmRlweQueryRequest fresnel_query_request;
  *fresnel_query_request.mutable_rlwe_query_request() = query_request;

  std::string request_body;
  fresnel_query_request.SerializeToString(&request_body);

  auto resource_request = GenerateResourceRequest(
      net::HttpRequestHeaders::kPostMethod, GetFresnelURL(), api_key_);

  // TODO(https://crbug.com/1266972): Refactor |url_loader_| network request
  // call to a shared helper method.
  url_loader_ = network ::SimpleURLLoader ::Create(
      std::move(resource_request), check_membership_traffic_annotation);
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(kQueryRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceActivityClient::OnCheckMembershipQueryDone,
                     weak_factory_.GetWeakPtr(), current_use_case),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckMembershipQueryDone(
    DeviceActiveUseCase* current_use_case,
    std::unique_ptr<std::string> response_body) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnCheckMembershipQueryDone);

  DCHECK_EQ(state_, State::kCheckingMembershipQuery);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);
  RecordPsmQueryResponseNetErrorCode(net_code);

  // Convert serialized response body to fresnel query response protobuf.
  FresnelPsmRlweQueryResponse psm_query_response;
  bool is_response_body_set = response_body.get() != nullptr;
  RecordIsPsmQueryResponseBodySet(is_response_body_set);

  if (!is_response_body_set ||
      !psm_query_response.ParseFromString(*response_body)) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::
            kQueryResponseBodyFailed);
    RecordIsPsmQueryResponseParsedCorrectly(false);

    TransitionToIdle(current_use_case);
    return;
  }

  // Query response body was parsed successfully.
  RecordIsPsmQueryResponseParsedCorrectly(true);

  // Parse |fresnel_query_response| for psm query_response.
  if (!psm_query_response.has_rlwe_query_response()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::
            kNotHasRlweQueryResponse);
    TransitionToIdle(current_use_case);
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryResponse query_response =
      psm_query_response.rlwe_query_response();
  auto status_or_response =
      current_use_case->GetPsmRlweClient()->ProcessQueryResponse(
          query_response);

  if (!status_or_response.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::
            kProcessQueryResponseFailed);
    TransitionToIdle(current_use_case);
    return;
  }

  // Ensure the existence of one membership response. Then, verify that it is
  // regarding the current PSM ID.
  psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
      status_or_response.value();

  if (rlwe_membership_responses.membership_responses_size() != 1) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::
            kMembershipResponsesSizeIsNotOne);
    TransitionToIdle(current_use_case);
    return;
  }

  private_membership::MembershipResponse membership_response =
      rlwe_membership_responses.membership_responses(0).membership_response();

  bool is_psm_id_member = membership_response.is_member();

  // Record the query membership result to UMA histogram.
  RecordQueryMembershipResultBoolean(is_psm_id_member);

  if (!is_psm_id_member) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    RecordCheckMembershipCases(
        DeviceActivityClient::CheckMembershipResponseCases::kIsNotPsmIdMember);
    TransitionToCheckIn(current_use_case);
    return;
  }

  // Update local state to signal ping has already been sent for use case
  // window.
  current_use_case->SetLastKnownPingTimestamp(
      last_transition_out_of_idle_time_);

  RecordCheckMembershipCases(
      DeviceActivityClient::CheckMembershipResponseCases::
          kSuccessfullySetLocalState);
  RecordDurationStateMetric(state_, state_timer_.Elapsed());
  TransitionToIdle(current_use_case);
  return;
}

void DeviceActivityClient::TransitionToCheckIn(
    DeviceActiveUseCase* current_use_case) {
  RecordTransitionToCheckInMinute();
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionToCheckIn);

  DCHECK(!url_loader_);

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("device_activity_client_check_in", R"(
        semantics {
          sender: "Device Activity"
          description:
                  "After checking that the Chrome device doesn't have the "
                  "membership of PSM, Chrome devices make an 'import network' "
                  "request which lets Fresnel Service import data into "
                  "PSM storage and Google web server Logs. Fresnel Service "
                  "is operating system images to be retrieved and provisioned "
                  "from anywhere internet access is available. So when a new "
                  "Chrome OS device joins a LAN, it gets added to the Private "
                  "Set of that LAN. After that, it can view the health status "
                  "(CPU/RAM/disk usage) of other Chrome OS devices "
                  "on the same LAN."
          trigger: "Chrome OS client makes this network request and records "
                   "the device activity when the default network changes"
          data: "Google API Key."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Not implemented."
        })");

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingIn;

  // Report UMA histogram for transitioning state to |kCheckingIn|.
  RecordStateCountMetric(state_);

  // Generate Fresnel PSM import request body.
  absl::optional<FresnelImportDataRequest> import_request =
      current_use_case->GenerateImportRequestBody();

  if (!import_request.has_value()) {
    TransitionToIdle(current_use_case);
    return;
  }

  std::string request_body;
  import_request.value().SerializeToString(&request_body);

  auto resource_request = GenerateResourceRequest(
      net::HttpRequestHeaders::kPostMethod, GetFresnelURL(), api_key_);

  // TODO(https://crbug.com/1266972): Refactor |url_loader_| network request
  // call to a shared helper method.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(kImportRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceActivityClient::OnCheckInDone,
                     weak_factory_.GetWeakPtr(), current_use_case),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckInDone(
    DeviceActiveUseCase* current_use_case,
    std::unique_ptr<std::string> response_body) {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientOnCheckInDone);

  DCHECK_EQ(state_, State::kCheckingIn);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);

  // Successful import request - PSM ID was imported successfully.
  if (net_code == net::OK) {
    // Update local state pref to record reporting device active.
    current_use_case->SetLastKnownPingTimestamp(
        last_transition_out_of_idle_time_);

    // Update churn local state and active status values after successful
    // churn use case ping (either cohort or observation).
    UpdateChurnLocalStateAfterCheckIn(current_use_case);
  }

  RecordDurationStateMetric(state_, state_timer_.Elapsed());
  TransitionToIdle(current_use_case);
}

void DeviceActivityClient::TransitionToIdle(
    DeviceActiveUseCase* current_use_case) {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientTransitionToIdle);

  DCHECK(!url_loader_);
  state_ = State::kIdle;

  if (current_use_case) {
    current_use_case->ClearSavedState();
    current_use_case = nullptr;

    // Pop the front of the queue since the use case has tried reporting.
    if (!pending_use_cases_.empty())
      pending_use_cases_.pop();
  }

  // Try to report the remaining pending use cases.
  if (!pending_use_cases_.empty()) {
    TransitionOutOfIdle(pending_use_cases_.front());
    return;
  }

  // Send DBus method to update last ping timestamps in preserved file.
  SaveLastPingDatesStatus();

  // Report UMA histogram for transitioning state back to |kIdle|.
  RecordStateCountMetric(state_);
}

}  // namespace ash::device_activity
