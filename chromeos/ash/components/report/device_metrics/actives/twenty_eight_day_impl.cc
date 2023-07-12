// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/actives/twenty_eight_day_impl.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/utils/device_metadata_utils.h"
#include "chromeos/ash/components/report/utils/network_utils.h"
#include "chromeos/ash/components/report/utils/psm_utils.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

namespace {

// PSM use case enum for 28-day-active use case.
constexpr psm_rlwe::RlweUseCase kPsmUseCase =
    psm_rlwe::RlweUseCase::CROS_FRESNEL_28DAY_ACTIVE;

// Size of rolling window of 28-day-active use case.
constexpr size_t kRollingWindowSize = 28;

}  // namespace

TwentyEightDayImpl::TwentyEightDayImpl(UseCaseParameters* params)
    : UseCase(params) {}

TwentyEightDayImpl::~TwentyEightDayImpl() = default;

void TwentyEightDayImpl::Run(base::OnceCallback<void()> callback) {
  callback_ = std::move(callback);

  if (!IsDevicePingRequired()) {
    std::move(callback_).Run();
    return;
  }

  // Perform check membership if the local state pref has default value.
  // This is done to avoid duplicate check in if the device pinged already.
  if (base::FeatureList::IsEnabled(
          features::kDeviceActiveClient28DayActiveCheckMembership) &&
      (GetLastPingTimestamp() == base::Time::UnixEpoch() ||
       GetLastPingTimestamp() == base::Time())) {
    CheckMembershipOprf();
  } else {
    CheckIn();
  }
}

void TwentyEightDayImpl::CheckMembershipOprf() {
  SetPsmRlweClient(kPsmUseCase, GetPsmIdentifiersToQuery());

  if (!GetPsmRlweClient()) {
    LOG(ERROR) << "Check membership failed since the PSM RLWE client could "
               << "not be initialized.";
    std::move(callback_).Run();
    return;
  }

  // Generate PSM Oprf request body.
  const auto status_or_oprf_request = GetPsmRlweClient()->CreateOprfRequest();
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
      base::BindOnce(&TwentyEightDayImpl::OnCheckMembershipOprfComplete,
                     weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void TwentyEightDayImpl::OnCheckMembershipOprfComplete(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();

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

void TwentyEightDayImpl::CheckMembershipQuery(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  // Generate PSM Query request body.
  const auto status_or_query_request =
      GetPsmRlweClient()->CreateQueryRequest(oprf_response);
  if (!status_or_query_request.ok()) {
    LOG(ERROR) << "Failed to create Query request.";
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
      base::BindOnce(&TwentyEightDayImpl::OnCheckMembershipQueryComplete,
                     weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void TwentyEightDayImpl::OnCheckMembershipQueryComplete(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();

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
      GetPsmRlweClient()->ProcessQueryResponse(query_response);

  if (!status_or_response.ok()) {
    LOG(ERROR) << "Failed to process query response.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
      status_or_response.value();

  if (rlwe_membership_responses.membership_responses_size() == 0) {
    LOG(ERROR) << "Check Membership for 28-day-active should query for greater "
               << "than 0 memberships. Size = "
               << rlwe_membership_responses.membership_responses_size();
    std::move(callback_).Run();
    return;
  }

  // TODO(hirthanan): Implement logic to find last ping 28 day.
  private_membership::MembershipResponse membership_response =
      rlwe_membership_responses.membership_responses(0).membership_response();

  bool is_psm_id_member = membership_response.is_member();

  if (is_psm_id_member) {
    LOG(ERROR) << "Check in ping was already sent earlier today.";
    SetLastPingTimestamp(GetParams()->GetActiveTs());
    std::move(callback_).Run();
    return;
  }

  CheckIn();
}

void TwentyEightDayImpl::CheckIn() {
  absl::optional<FresnelImportDataRequest> import_request =
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
  url_loader_->DownloadToString(
      GetParams()->GetUrlLoaderFactory().get(),
      base::BindOnce(&TwentyEightDayImpl::OnCheckInComplete,
                     weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void TwentyEightDayImpl::OnCheckInComplete(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();

  if (net_code == net::OK) {
    // Update local state pref to record reporting device active.
    SetLastPingTimestamp(GetParams()->GetActiveTs());
  } else {
    LOG(ERROR) << "Failed to check in successfully. Net code = " << net_code;
  }

  // Check-in completed - use case is done running.
  std::move(callback_).Run();
}

base::Time TwentyEightDayImpl::GetLastPingTimestamp() {
  return GetParams()->GetLocalState()->GetTime(
      ash::report::prefs::kDeviceActiveLastKnown28DayActivePingTimestamp);
}

void TwentyEightDayImpl::SetLastPingTimestamp(base::Time ts) {
  GetParams()->GetLocalState()->SetTime(
      ash::report::prefs::kDeviceActiveLastKnown28DayActivePingTimestamp, ts);
}

std::vector<psm_rlwe::RlwePlaintextId>
TwentyEightDayImpl::GetPsmIdentifiersToQuery() {
  std::string window_id =
      utils::TimeToYYYYMMDDString(GetParams()->GetActiveTs());

  absl::optional<psm_rlwe::RlwePlaintextId> psm_id =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id);

  if (!psm_id.has_value()) {
    LOG(ERROR) << "Failed to generate PSM ID to query.";
    return {};
  }

  auto psm_id_with_non_sensitive_slice = psm_id.value();
  psm_id_with_non_sensitive_slice.set_allocated_non_sensitive_id(&window_id);

  // TODO: implement methods for psm_ids to query for 28DA use case.
  // IMPORTANT: Queried ID's must attach non_sensitive_id to query correctly.
  std::vector<psm_rlwe::RlwePlaintextId> query_psm_ids = {psm_id.value()};
  return query_psm_ids;
}

absl::optional<FresnelImportDataRequest>
TwentyEightDayImpl::GenerateImportRequestBody() {
  // Generate Fresnel PSM import request body.
  FresnelImportDataRequest import_request;
  import_request.set_use_case(kPsmUseCase);

  // Certain metadata is passed by chrome, since it's not available in ash.
  version_info::Channel version_channel =
      GetParams()->GetChromeDeviceParams().chrome_channel;
  ash::report::MarketSegment market_segment =
      GetParams()->GetChromeDeviceParams().market_segment;

  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chromeos_version(utils::GetChromeMilestone());
  device_metadata->set_hardware_id(utils::GetFullHardwareClass());
  device_metadata->set_chromeos_channel(
      utils::GetChromeChannel(version_channel));
  device_metadata->set_market_segment(market_segment);

  // Normalize current ts and last known ts to midnight.
  // Not doing so will cause missing imports depending on the HH/MM/SS.
  base::Time cur_ping_ts_midnight = GetParams()->GetActiveTs().UTCMidnight();
  base::Time last_ping_ts_midnight = GetLastPingTimestamp().UTCMidnight();

  // Iterate from days [cur_ts, cur_ts+27], which represents the 28 day window.
  for (int i = 0; i < static_cast<int>(kRollingWindowSize); i++) {
    base::Time day_n = cur_ping_ts_midnight + base::Days(i);

    // Only generate import data for new identifiers to import.
    // last_known_ping_ts + 27 gives us the last day we previously sent an
    // import data request for.
    if (day_n <= (last_ping_ts_midnight + base::Days(kRollingWindowSize - 1))) {
      continue;
    }

    std::string window_id = utils::TimeToYYYYMMDDString(day_n);
    absl::optional<psm_rlwe::RlwePlaintextId> psm_id =
        utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                     psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                     window_id);

    if (window_id.empty() || !psm_id.has_value()) {
      LOG(ERROR) << "Window ID or Psm ID is empty.";
      return absl::nullopt;
    }

    FresnelImportData* import_data = import_request.add_import_data();
    import_data->set_window_identifier(window_id);
    import_data->set_plaintext_id(psm_id.value().sensitive_id());
    import_data->set_is_pt_window_identifier(true);
  }

  return import_request;
}

bool TwentyEightDayImpl::IsDevicePingRequired() {
  base::Time last_ping_ts = GetLastPingTimestamp();
  base::Time cur_ping_ts = GetParams()->GetActiveTs();

  // Safety check to avoid against clock drift, or unexpected timestamps.
  // Check should make sure that we are not reporting window id's for
  // day's previous to one that we reported already.
  if (last_ping_ts >= cur_ping_ts) {
    return false;
  }

  return utils::TimeToYYYYMMDDString(last_ping_ts) !=
         utils::TimeToYYYYMMDDString(cur_ping_ts);
}

}  // namespace ash::report::device_metrics
