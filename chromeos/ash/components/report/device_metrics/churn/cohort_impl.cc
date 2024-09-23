// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/churn/cohort_impl.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/report/device_metrics/churn/active_status.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/utils/device_metadata_utils.h"
#include "chromeos/ash/components/report/utils/network_utils.h"
#include "chromeos/ash/components/report/utils/psm_utils.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "chromeos/ash/components/report/utils/uma_utils.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

namespace {

// PSM use case enum for churn monthly cohort use case.
constexpr psm_rlwe::RlweUseCase kPsmUseCase =
    psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT;

}  // namespace

CohortImpl::CohortImpl(UseCaseParameters* params)
    : UseCase(params),
      active_status_(std::make_unique<ActiveStatus>(params->GetLocalState())) {}

CohortImpl::~CohortImpl() = default;

void CohortImpl::Run(base::OnceCallback<void()> callback) {
  callback_ = std::move(callback);

  if (!IsDevicePingRequired()) {
    utils::RecordIsDevicePingRequired(utils::PsmUseCase::kCohort, false);
    std::move(callback_).Run();
    return;
  }

  utils::RecordIsDevicePingRequired(utils::PsmUseCase::kCohort, true);

  // Perform check membership if the local state pref has default value.
  // This is done to avoid duplicate check in if the device pinged already.
  if (base::FeatureList::IsEnabled(
          features::kDeviceActiveClientChurnCohortCheckMembership) &&
      (GetLastPingTimestamp() == base::Time::UnixEpoch() ||
       GetLastPingTimestamp() == base::Time())) {
    CheckMembershipOprf();
  } else {
    CheckIn();
  }
}

base::WeakPtr<CohortImpl> CohortImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void CohortImpl::CheckMembershipOprf() {
  PsmClientManager* psm_client_manager = GetParams()->GetPsmClientManager();

  psm_client_manager->SetPsmRlweClient(kPsmUseCase, GetPsmIdentifiersToQuery());

  if (!psm_client_manager->GetPsmRlweClient()) {
    LOG(ERROR) << "Check membership failed since the PSM RLWE client could "
               << "not be initialized.";
    std::move(callback_).Run();
    return;
  }

  // Generate PSM Oprf request body.
  const auto status_or_oprf_request = psm_client_manager->CreateOprfRequest();
  if (!status_or_oprf_request.ok()) {
    LOG(ERROR) << "Failed to create OPRF request.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfRequest oprf_request =
      status_or_oprf_request.value();

  // Wrap PSM Oprf request body by FresnelPsmRlweOprfRequest proto.
  // This proto is expected by the Fresnel service.
  report::FresnelPsmRlweOprfRequest fresnel_oprf_request;
  *fresnel_oprf_request.mutable_rlwe_oprf_request() = oprf_request;

  std::string request_body;
  fresnel_oprf_request.SerializeToString(&request_body);

  auto resource_request =
      utils::GenerateResourceRequest(utils::GetOprfRequestURL());

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetCheckMembershipTrafficTag());
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(utils::GetOprfRequestTimeout());
  url_loader_->DownloadToString(
      GetParams()->GetUrlLoaderFactory().get(),
      base::BindOnce(&CohortImpl::OnCheckMembershipOprfComplete,
                     weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void CohortImpl::OnCheckMembershipOprfComplete(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::kCohort,
                            utils::PsmRequest::kOprf, net_code);

  // Convert serialized response body to oprf response protobuf.
  FresnelPsmRlweOprfResponse psm_oprf_response;
  bool is_response_body_set = response_body.get() != nullptr;

  if (!is_response_body_set ||
      !psm_oprf_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Oprf response net code = " << net_code;
    LOG(ERROR) << "Response body was not set or could not be parsed into "
               << "FresnelPsmRlweOprfResponse proto. "
               << "Is response body set = " << is_response_body_set;
    std::move(callback_).Run();
    return;
  }

  if (!psm_oprf_response.has_rlwe_oprf_response()) {
    LOG(ERROR) << "Oprf response net code = " << net_code;
    LOG(ERROR) << "FresnelPsmRlweOprfResponse is missing the actual oprf "
                  "response from server.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
      psm_oprf_response.rlwe_oprf_response();

  CheckMembershipQuery(oprf_response);
}

void CohortImpl::CheckMembershipQuery(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  PsmClientManager* psm_client_manager = GetParams()->GetPsmClientManager();

  // Generate PSM Query request body.
  const auto status_or_query_request =
      psm_client_manager->CreateQueryRequest(oprf_response);
  if (!status_or_query_request.ok()) {
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryRequest query_request =
      status_or_query_request.value();

  // Wrap PSM Query request body by FresnelPsmRlweQueryRequest proto.
  // This proto is expected by the Fresnel service.
  report::FresnelPsmRlweQueryRequest fresnel_query_request;
  *fresnel_query_request.mutable_rlwe_query_request() = query_request;

  std::string request_body;
  fresnel_query_request.SerializeToString(&request_body);

  auto resource_request =
      utils::GenerateResourceRequest(utils::GetQueryRequestURL());

  url_loader_ = network ::SimpleURLLoader ::Create(
      std::move(resource_request), GetCheckMembershipTrafficTag());
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(utils::GetQueryRequestTimeout());
  url_loader_->DownloadToString(
      GetParams()->GetUrlLoaderFactory().get(),
      base::BindOnce(&CohortImpl::OnCheckMembershipQueryComplete,
                     weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void CohortImpl::OnCheckMembershipQueryComplete(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::kCohort,
                            utils::PsmRequest::kQuery, net_code);

  // Convert serialized response body to fresnel query response protobuf.
  FresnelPsmRlweQueryResponse psm_query_response;
  bool is_response_body_set = response_body.get() != nullptr;

  if (!is_response_body_set ||
      !psm_query_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Query response net code = " << net_code;
    LOG(ERROR) << "Response body was not set or could not be parsed into "
               << "FresnelPsmRlweQueryResponse proto. "
               << "Is response body set = " << is_response_body_set;
    std::move(callback_).Run();
    return;
  }

  if (!psm_query_response.has_rlwe_query_response()) {
    LOG(ERROR) << "Query response net code = " << net_code;
    LOG(ERROR) << "FresnelPsmRlweQueryResponse is missing the actual query "
                  "response from server.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryResponse query_response =
      psm_query_response.rlwe_query_response();
  auto status_or_response =
      GetParams()->GetPsmClientManager()->ProcessQueryResponse(query_response);

  if (!status_or_response.ok()) {
    LOG(ERROR) << "Failed to process query response.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
      status_or_response.value();

  // TODO: Update logic below here to handle cohort check membership...
  if (rlwe_membership_responses.membership_responses_size() == 0) {
    LOG(ERROR) << "Check Membership for Cohort should query for greater "
               << "than 0 memberships. Size = "
               << rlwe_membership_responses.membership_responses_size();
    std::move(callback_).Run();
    return;
  }

  LOG(ERROR) << "TODO: Implement logic to find last ping Cohort use case. ";
  private_membership::MembershipResponse membership_response =
      rlwe_membership_responses.membership_responses(0).membership_response();

  bool is_psm_id_member = membership_response.is_member();

  if (is_psm_id_member) {
    LOG(ERROR) << "Check In ping was already sent this month.";
    SetLastPingTimestamp(GetParams()->GetActiveTs());
    std::move(callback_).Run();
    return;
  }

  CheckIn();
}

void CohortImpl::CheckIn() {
  std::optional<FresnelImportDataRequest> import_request =
      GenerateImportRequestBody();
  if (!import_request.has_value()) {
    LOG(ERROR) << "Failed to create the import request body.";
    std::move(callback_).Run();
    return;
  }

  std::string request_body;
  import_request.value().SerializeToString(&request_body);

  auto resource_request =
      utils::GenerateResourceRequest(utils::GetImportRequestURL());

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 GetCheckInTrafficTag());
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(utils::GetImportRequestTimeout());
  url_loader_->DownloadToString(GetParams()->GetUrlLoaderFactory().get(),
                                base::BindOnce(&CohortImpl::OnCheckInComplete,
                                               weak_factory_.GetWeakPtr()),
                                utils::GetMaxFresnelResponseSizeBytes());
}

void CohortImpl::OnCheckInComplete(std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::kCohort,
                            utils::PsmRequest::kImport, net_code);

  if (net_code == net::OK) {
    UpdateLocalStateOnCheckInSuccess();
  } else {
    LOG(ERROR) << "Failed to check in successfully. Net code = " << net_code;
  }

  // Check-in completed - use case is done running.
  std::move(callback_).Run();
}

base::Time CohortImpl::GetLastPingTimestamp() {
  return GetParams()->GetLocalState()->GetTime(
      ash::report::prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp);
}

void CohortImpl::SetLastPingTimestamp(base::Time ts) {
  GetParams()->GetLocalState()->SetTime(
      ash::report::prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp, ts);
}

std::vector<psm_rlwe::RlwePlaintextId> CohortImpl::GetPsmIdentifiersToQuery() {
  // TODO: implement methods to generate PSM id.
  std::vector<psm_rlwe::RlwePlaintextId> query_psm_ids = {};
  return query_psm_ids;
}

std::optional<FresnelImportDataRequest>
CohortImpl::GenerateImportRequestBody() {
  FresnelImportDataRequest import_request;
  import_request.set_use_case(kPsmUseCase);

  // Certain metadata is passed by chrome, since it's not available in ash.
  version_info::Channel version_channel =
      GetParams()->GetChromeDeviceParams().chrome_channel;
  ash::report::MarketSegment market_segment =
      GetParams()->GetChromeDeviceParams().market_segment;

  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chrome_milestone(utils::GetChromeMilestone());
  device_metadata->set_hardware_id(utils::GetFullHardwareClass());
  device_metadata->set_chromeos_channel(
      utils::GetChromeChannel(version_channel));
  device_metadata->set_market_segment(market_segment);

  std::string window_id = utils::TimeToYYYYMMString(GetParams()->GetActiveTs());
  std::optional<psm_rlwe::RlwePlaintextId> psm_id =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id);

  if (window_id.empty() || !psm_id.has_value()) {
    LOG(ERROR) << "Window ID or Psm ID is empty.";
    return std::nullopt;
  }

  FresnelImportData* import_data = import_request.add_import_data();
  import_data->set_window_identifier(window_id);
  import_data->set_plaintext_id(psm_id.value().sensitive_id());
  import_data->set_is_pt_window_identifier(true);

  ChurnCohortMetadata* cohort_metadata =
      import_data->mutable_churn_cohort_metadata();
  std::optional<ChurnCohortMetadata> new_cohort_metadata =
      active_status_->CalculateCohortMetadata(GetParams()->GetActiveTs());
  if (!new_cohort_metadata.has_value()) {
    LOG(ERROR) << "Failed to calculate new cohort metadata.";
    return std::nullopt;
  }

  base::Time active_ts = GetParams()->GetActiveTs();
  std::optional<base::Time> first_active_week_ts = utils::GetFirstActiveWeek();

  if (!first_active_week_ts.has_value() ||
      first_active_week_ts.value() == base::Time() ||
      first_active_week_ts.value() == base::Time::UnixEpoch()) {
    LOG(ERROR) << "Failed to retrieve first active week from VPD. "
                  "Setting first active week to UNKNOWN.";
    cohort_metadata->set_first_active_week("UNKNOWN");
  } else {
    int max_days_in_5_weeks = 7 * 5;
    bool within_date_range = utils::IsFirstActiveUnderNDaysAgo(
        active_ts, first_active_week_ts.value(), max_days_in_5_weeks);

    // Privacy approved 5 weeks of first active week history in cohort ping.
    // In order for analysts to avoid double counting on the server-side.
    if (within_date_range) {
      cohort_metadata->set_first_active_week(
          utils::ConvertTimeToISO8601String(first_active_week_ts.value()));
    }
  }

  *cohort_metadata = new_cohort_metadata.value();
  return import_request;
}

void CohortImpl::UpdateLocalStateOnCheckInSuccess() {
  // Check the new cohort active status value is valid.
  std::optional<int> new_active_val =
      active_status_->CalculateNewValue(GetParams()->GetActiveTs());
  if (!new_active_val.has_value()) {
    LOG(ERROR)
        << "Failed to update active status value after successful cohort "
           "check in.";
    return;
  }

  PrefService* local_state = GetParams()->GetLocalState();

  // Read the current relative observation period reported status.
  bool is_active_current_period_minus_0 = local_state->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0);
  bool is_active_current_period_minus_1 = local_state->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1);
  bool is_active_current_period_minus_2 = local_state->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2);

  int active_val = active_status_->GetValue();

  // Shift observation periods relative to the last reported cohort month.
  // NOTE: The observation periods are relative to the last cohort ping month.
  // Shift occurs after a successful churn cohort ping in a new month to
  // account for the relative observation report statuses.
  for (int i = 0; i < 3; i++) {
    if (active_val == new_active_val.value()) {
      break;
    }

    is_active_current_period_minus_2 = is_active_current_period_minus_1;
    is_active_current_period_minus_1 = is_active_current_period_minus_0;
    is_active_current_period_minus_0 = false;

    active_val += 1;
  }

  // Update the local state keys after determining all new values.
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0,
      is_active_current_period_minus_0);
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1,
      is_active_current_period_minus_1);
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2,
      is_active_current_period_minus_2);
  active_status_->SetValue(new_active_val.value());
  SetLastPingTimestamp(GetParams()->GetActiveTs());
}

bool CohortImpl::IsDevicePingRequired() {
  base::Time last_ping_ts = GetLastPingTimestamp();
  base::Time cur_ping_ts = GetParams()->GetActiveTs();

  // Safety check to avoid against clock drift, or unexpected timestamps.
  // Check should make sure that we are not reporting window id's for
  // a month previous to one that we reported already.
  if (last_ping_ts >= cur_ping_ts) {
    return false;
  }

  return utils::TimeToYYYYMMString(last_ping_ts) !=
         utils::TimeToYYYYMMString(cur_ping_ts);
}

}  // namespace ash::report::device_metrics
