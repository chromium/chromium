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
#include "chromeos/ash/components/report/utils/uma_utils.h"
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
    : UseCase(params) {
  LoadActivesCachePref();
}

TwentyEightDayImpl::~TwentyEightDayImpl() = default;

void TwentyEightDayImpl::Run(base::OnceCallback<void()> callback) {
  FilterActivesCache();
  callback_ = std::move(callback);

  if (!IsDevicePingRequired()) {
    utils::RecordIsDevicePingRequired(utils::PsmUseCase::k28DA, false);
    std::move(callback_).Run();
    return;
  }

  utils::RecordIsDevicePingRequired(utils::PsmUseCase::k28DA, true);

  // Perform check membership if the local state pref has default value.
  // This is done to avoid duplicate check in if the device pinged already.
  if (base::FeatureList::IsEnabled(
          features::kDeviceActiveClient28DayActiveCheckMembership) &&
      (GetLastPingTimestamp() == base::Time::UnixEpoch() ||
       GetLastPingTimestamp() == base::Time())) {
    CheckMembershipOprfFirstPhase();
  } else {
    CheckIn();
  }
}

base::WeakPtr<TwentyEightDayImpl> TwentyEightDayImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void TwentyEightDayImpl::CheckMembershipOprf() {
  NOTREACHED_IN_MIGRATION();
  return;
}

void TwentyEightDayImpl::OnCheckMembershipOprfComplete(
    std::unique_ptr<std::string> response_body) {
  NOTREACHED_IN_MIGRATION();
  return;
}

void TwentyEightDayImpl::CheckMembershipQuery(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  NOTREACHED_IN_MIGRATION();
  return;
}

void TwentyEightDayImpl::OnCheckMembershipQueryComplete(
    std::unique_ptr<std::string> response_body) {
  NOTREACHED_IN_MIGRATION();
  return;
}

void TwentyEightDayImpl::CheckIn() {
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
  url_loader_->DownloadToString(
      GetParams()->GetUrlLoaderFactory().get(),
      base::BindOnce(&TwentyEightDayImpl::OnCheckInCompleteCustom,
                     weak_factory_.GetWeakPtr(), import_request.value()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void TwentyEightDayImpl::OnCheckInComplete(
    std::unique_ptr<std::string> response_body) {
  NOTREACHED_IN_MIGRATION();
  return;
}

void TwentyEightDayImpl::OnCheckInCompleteCustom(
    const FresnelImportDataRequest import_request,
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::k28DA,
                            utils::PsmRequest::kImport, net_code);

  if (net_code == net::OK) {
    // Update local state pref to record reporting device active.
    SetLastPingTimestamp(GetParams()->GetActiveTs());

    // Update |actives_cache_| with the newly import window ids.
    for (const auto& import_data : import_request.import_data()) {
      if (import_data.has_window_identifier()) {
        actives_cache_.Set(import_data.window_identifier(), true);
      }
    }
    SaveActivesCachePref();
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
  return GetPsmIdentifiersToQueryPhaseOne();
}

std::optional<FresnelImportDataRequest>
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
  device_metadata->set_chrome_milestone(utils::GetChromeMilestone());
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

void TwentyEightDayImpl::LoadActivesCachePref() {
  const base::Value::Dict& actives_pref = GetParams()->GetLocalState()->GetDict(
      prefs::kDeviceActive28DayActivePingCache);
  actives_cache_ = actives_pref.Clone();
}

void TwentyEightDayImpl::SaveActivesCachePref() {
  GetParams()->GetLocalState()->SetDict(
      prefs::kDeviceActive28DayActivePingCache, actives_cache_.Clone());
}

void TwentyEightDayImpl::FilterActivesCache() {
  base::Time day_0 = GetParams()->GetActiveTs().UTCMidnight();
  base::Time day_27 = day_0 + base::Days(27);

  base::Value::Dict new_actives_cache;

  // Remove active cache entries if not between [day_0, day_27] inclusive.
  for (base::Time day = day_0; day <= day_27; day += base::Days(1)) {
    std::string day_window_id = utils::TimeToYYYYMMDDString(day);

    std::optional<bool> result = actives_cache_.FindBool(day_window_id);
    if (result.has_value()) {
      new_actives_cache.Set(day_window_id, result.value());
    }
  }

  // Update actives_cache_ with the filtered entries.
  actives_cache_ = std::move(new_actives_cache);
  SaveActivesCachePref();
}

base::Time TwentyEightDayImpl::FindLeftMostKnownMembership() {
  base::Time day_0 = GetParams()->GetActiveTs().UTCMidnight();
  base::Time day_27 = day_0 + base::Days(27);

  // Find the left most index, which had a positive check membership response.
  base::Time left_ts = base::Time::UnixEpoch();
  for (base::Time day = day_0; day <= day_27; day += base::Days(1)) {
    std::string day_window_id = utils::TimeToYYYYMMDDString(day);
    std::optional<bool> is_member = actives_cache_.FindBool(day_window_id);

    if (is_member.has_value() && is_member.value()) {
      left_ts = std::max(day, left_ts);
    }
  }

  return left_ts;
}

base::Time TwentyEightDayImpl::FindRightMostKnownNonMembership() {
  base::Time day_0 = GetParams()->GetActiveTs().UTCMidnight();
  base::Time day_27 = day_0 + base::Days(27);

  // Find the right most index, which had a negative check membership response.
  base::Time right_ts = day_27;
  for (base::Time day = day_27; day >= day_0; day -= base::Days(1)) {
    std::string day_window_id = utils::TimeToYYYYMMDDString(day);
    std::optional<bool> is_member = actives_cache_.FindBool(day_window_id);

    if (is_member.has_value() && !is_member.value()) {
      right_ts = std::min(day, right_ts);
    }
  }

  return right_ts;
}

bool TwentyEightDayImpl::IsFirstPhaseComplete() {
  // 28 day window represented by days between [0, 27].
  base::Time day_0 = GetParams()->GetActiveTs().UTCMidnight();
  base::Time day_26 =
      (GetParams()->GetActiveTs() + base::Days(26)).UTCMidnight();
  base::Time day_27 =
      (GetParams()->GetActiveTs() + base::Days(27)).UTCMidnight();

  std::string window_id_day_0 = utils::TimeToYYYYMMDDString(day_0);
  std::string window_id_day_26 = utils::TimeToYYYYMMDDString(day_26);
  std::string window_id_day_27 = utils::TimeToYYYYMMDDString(day_27);

  return actives_cache_.contains(window_id_day_27) ||
         actives_cache_.contains(window_id_day_26) ||
         actives_cache_.contains(window_id_day_0);
}

std::vector<psm_rlwe::RlwePlaintextId>
TwentyEightDayImpl::GetPsmIdentifiersToQueryPhaseOne() {
  // 28 day window represented by days between [0, 27].
  base::Time day_0 = GetParams()->GetActiveTs().UTCMidnight();
  base::Time day_26 =
      (GetParams()->GetActiveTs() + base::Days(26)).UTCMidnight();
  base::Time day_27 =
      (GetParams()->GetActiveTs() + base::Days(27)).UTCMidnight();

  std::string window_id_day_0 = utils::TimeToYYYYMMDDString(day_0);
  std::string window_id_day_26 = utils::TimeToYYYYMMDDString(day_26);
  std::string window_id_day_27 = utils::TimeToYYYYMMDDString(day_27);

  std::optional<psm_rlwe::RlwePlaintextId> psm_id_day_0 =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id_day_0);
  std::optional<psm_rlwe::RlwePlaintextId> psm_id_day_26 =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id_day_26);
  std::optional<psm_rlwe::RlwePlaintextId> psm_id_day_27 =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id_day_27);

  if (!psm_id_day_0.has_value() || !psm_id_day_26.has_value() ||
      !psm_id_day_27.has_value()) {
    LOG(ERROR) << "Failed to generate PSM ID to query.";
    return {};
  }

  // IMPORTANT: Queried ID's must attach non_sensitive_id to query correctly.
  auto psm_id_0_with_non_sensitive_slice = psm_id_day_0.value();
  auto psm_id_26_with_non_sensitive_slice = psm_id_day_26.value();
  auto psm_id_27_with_non_sensitive_slice = psm_id_day_27.value();
  psm_id_0_with_non_sensitive_slice.set_non_sensitive_id(window_id_day_0);
  psm_id_26_with_non_sensitive_slice.set_non_sensitive_id(window_id_day_26);
  psm_id_27_with_non_sensitive_slice.set_non_sensitive_id(window_id_day_27);

  // Only query unknown PSM identifiers for the 28DA use case.
  std::vector<psm_rlwe::RlwePlaintextId> query_psm_ids;
  if (!actives_cache_.contains(window_id_day_0)) {
    query_psm_ids.emplace_back(psm_id_0_with_non_sensitive_slice);
  }
  if (!actives_cache_.contains(window_id_day_26)) {
    query_psm_ids.emplace_back(psm_id_26_with_non_sensitive_slice);
  }
  if (!actives_cache_.contains(window_id_day_27)) {
    query_psm_ids.emplace_back(psm_id_27_with_non_sensitive_slice);
  }

  return query_psm_ids;
}

void TwentyEightDayImpl::CheckMembershipOprfFirstPhase() {
  DCHECK(!url_loader_);

  PsmClientManager* psm_client_manager = GetParams()->GetPsmClientManager();

  psm_client_manager->SetPsmRlweClient(kPsmUseCase,
                                       GetPsmIdentifiersToQueryPhaseOne());

  if (!psm_client_manager->GetPsmRlweClient()) {
    LOG(ERROR) << "First phase of check membership failed since the "
               << "PSM RLWE client could not be initialized.";
    std::move(callback_).Run();
    return;
  }

  // Generate PSM Oprf request body.
  const auto status_or_oprf_request = psm_client_manager->CreateOprfRequest();
  if (!status_or_oprf_request.ok()) {
    LOG(ERROR) << "Failed to create first phase OPRF request.";
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
      base::BindOnce(
          &TwentyEightDayImpl::OnCheckMembershipOprfCompleteFirstPhase,
          weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void TwentyEightDayImpl::OnCheckMembershipOprfCompleteFirstPhase(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::k28DA, utils::PsmRequest::kOprf,
                            net_code);

  // Convert serialized response body to oprf response protobuf.
  FresnelPsmRlweOprfResponse psm_oprf_response;
  bool is_response_body_set = response_body.get() != nullptr;

  if (!is_response_body_set ||
      !psm_oprf_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "First phase OPRF response net code = " << net_code;
    LOG(ERROR) << "Response body was not set or could not be parsed into "
               << "FresnelPsmRlweOprfResponse proto. "
               << "Is response body set = " << is_response_body_set;
    std::move(callback_).Run();
    return;
  }

  if (!psm_oprf_response.has_rlwe_oprf_response()) {
    LOG(ERROR) << "First phase OPRF response net code = " << net_code;
    LOG(ERROR) << "FresnelPsmRlweOprfResponse is missing the actual oprf "
                  "response from server.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
      psm_oprf_response.rlwe_oprf_response();

  CheckMembershipQueryFirstPhase(oprf_response);
}

void TwentyEightDayImpl::CheckMembershipQueryFirstPhase(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  DCHECK(!url_loader_);

  PsmClientManager* psm_client_manager = GetParams()->GetPsmClientManager();

  // Generate PSM Query request body.
  const auto status_or_query_request =
      psm_client_manager->CreateQueryRequest(oprf_response);
  if (!status_or_query_request.ok()) {
    LOG(ERROR) << "First phrase failed to create Query request.";
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
      base::BindOnce(
          &TwentyEightDayImpl::OnCheckMembershipQueryCompleteFirstPhase,
          weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void TwentyEightDayImpl::OnCheckMembershipQueryCompleteFirstPhase(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::k28DA, utils::PsmRequest::kQuery,
                            net_code);

  // Convert serialized response body to fresnel query response protobuf.
  FresnelPsmRlweQueryResponse psm_query_response;
  bool is_response_body_set = response_body.get() != nullptr;

  if (!is_response_body_set ||
      !psm_query_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "First phase query response net code = " << net_code;
    LOG(ERROR) << "Response body was not set or could not be parsed into "
               << "FresnelPsmRlweQueryResponse proto. "
               << "Is response body set = " << is_response_body_set;
    std::move(callback_).Run();
    return;
  }

  if (!psm_query_response.has_rlwe_query_response()) {
    LOG(ERROR) << "First phase query response net code = " << net_code;
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
    LOG(ERROR) << "First phase failed to process query response.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
      status_or_response.value();

  if (rlwe_membership_responses.membership_responses_size() == 0) {
    LOG(ERROR) << "First phase of check Membership for 28-day-active should "
                  "query for greater than 0 memberships. Size = "
               << rlwe_membership_responses.membership_responses_size();
    std::move(callback_).Run();
    return;
  }

  // Compute day 0, 26, and 27 psm id's which are used for comparison below.
  base::Time day_0 = GetParams()->GetActiveTs().UTCMidnight();
  base::Time day_26 =
      (GetParams()->GetActiveTs() + base::Days(26)).UTCMidnight();
  base::Time day_27 =
      (GetParams()->GetActiveTs() + base::Days(27)).UTCMidnight();

  std::string window_id_day_0 = utils::TimeToYYYYMMDDString(day_0);
  std::string window_id_day_26 = utils::TimeToYYYYMMDDString(day_26);
  std::string window_id_day_27 = utils::TimeToYYYYMMDDString(day_27);

  std::optional<psm_rlwe::RlwePlaintextId> psm_id_day_0 =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id_day_0);

  std::optional<psm_rlwe::RlwePlaintextId> psm_id_day_26 =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id_day_26);

  std::optional<psm_rlwe::RlwePlaintextId> psm_id_day_27 =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id_day_27);

  // Update local state based on check membership response from first phase.
  for (auto& response : rlwe_membership_responses.membership_responses()) {
    psm_rlwe::RlwePlaintextId searched_psm_id = response.plaintext_id();
    bool is_psm_id_member = response.membership_response().is_member();

    if (psm_id_day_0.has_value() &&
        psm_id_day_0.value().sensitive_id() == searched_psm_id.sensitive_id()) {
      LOG_IF(ERROR, is_psm_id_member)
          << "Check in ping was already sent earlier today for window = "
          << window_id_day_0;
      actives_cache_.Set(window_id_day_0, is_psm_id_member);
    }

    if (psm_id_day_26.has_value() && psm_id_day_26.value().sensitive_id() ==
                                         searched_psm_id.sensitive_id()) {
      LOG_IF(ERROR, is_psm_id_member)
          << "Check in ping was already sent earlier today for window = "
          << window_id_day_26;
      actives_cache_.Set(window_id_day_26, is_psm_id_member);
    }

    if (psm_id_day_27.has_value() && psm_id_day_27.value().sensitive_id() ==
                                         searched_psm_id.sensitive_id()) {
      LOG_IF(ERROR, is_psm_id_member)
          << "Check in ping was already sent earlier today for window = "
          << window_id_day_27;
      actives_cache_.Set(window_id_day_27, is_psm_id_member);
    }
  }
  SaveActivesCachePref();

  DCHECK(actives_cache_.contains(window_id_day_0) &&
         actives_cache_.contains(window_id_day_26) &&
         actives_cache_.contains(window_id_day_27));

  bool is_day_0_member = actives_cache_.FindBool(window_id_day_0).value();
  bool is_day_26_member = actives_cache_.FindBool(window_id_day_26).value();
  bool is_day_27_member = actives_cache_.FindBool(window_id_day_27).value();

  // Handle logic on whether to check-in, or whether second phase is required.
  if (!is_day_0_member) {
    SetLastPingTimestamp(day_0 - base::Days(28));
    CheckIn();
    return;
  } else if (is_day_27_member) {
    SetLastPingTimestamp(day_0);
    LOG(ERROR) << "First phase - device had already pinged today.";
    std::move(callback_).Run();
    return;
  } else if (is_day_26_member) {
    LOG(ERROR) << "First phase - device last pinged yesterday.";
    SetLastPingTimestamp(day_0 - base::Days(1));
    CheckIn();
    return;
  } else {
    CheckMembershipOprfSecondPhase();
    return;
  }
}

// Proxy to check if second phase is completed.
bool TwentyEightDayImpl::IsSecondPhaseComplete() {
  // Second phase performs up to 5 binary searches between days [1, 26].
  // 28 day window represented by days between [0, 27].
  base::Time day_0 = GetParams()->GetActiveTs().UTCMidnight();
  base::Time day_27 = day_0 + base::Days(27);
  std::string window_id_day_0 = utils::TimeToYYYYMMDDString(day_0);
  std::string window_id_day_27 = utils::TimeToYYYYMMDDString(day_27);

  std::optional<bool> is_member_day_0 =
      actives_cache_.FindBool(window_id_day_0);
  std::optional<bool> is_member_day_27 =
      actives_cache_.FindBool(window_id_day_27);

  if ((is_member_day_0.has_value() && !is_member_day_0.value()) ||
      (is_member_day_27.has_value() && is_member_day_27.value())) {
    LOG(ERROR) << "Second phase is not needed if day 0 is known to be false "
               << "or day 27 is known to be true.";
    return true;
  }

  // Leftmost membership ts should be 1 day before rightmost non-membership ts.
  return (FindLeftMostKnownMembership() + base::Days(1)) ==
         FindRightMostKnownNonMembership();
}

std::vector<psm_rlwe::RlwePlaintextId>
TwentyEightDayImpl::GetPsmIdentifiersToQueryPhaseTwo() {
  // Find left and right unknown bounds to check membership between.
  base::Time left_ts = FindLeftMostKnownMembership() + base::Days(1);
  base::Time right_ts = FindRightMostKnownNonMembership() - base::Days(1);

  if (left_ts > right_ts) {
    LOG(ERROR) << "Invalid actives cache values. Leftmost known membership = "
               << left_ts << ". Rightmost known non-membership = " << right_ts;
    return {};
  }

  // TODO(hirthanan): Thoroughly evaluate different scenarios of calculations.
  base::TimeDelta time_diff = right_ts - left_ts;
  base::Time query_day = (left_ts + time_diff / 2).UTCMidnight();
  std::string window_id_query_day = utils::TimeToYYYYMMDDString(query_day);

  if (actives_cache_.contains(window_id_query_day)) {
    NOTREACHED_IN_MIGRATION()
        << "Unexpectedly the Window ID is contained in the actives "
           "cache already = "
        << window_id_query_day;
    return {};
  }

  std::optional<psm_rlwe::RlwePlaintextId> psm_id_query_day =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id_query_day);
  if (!psm_id_query_day.has_value()) {
    LOG(ERROR) << "Failed to generate PSM ID to query.";
    return {};
  }

  // IMPORTANT: Queried ID's must attach non_sensitive_id to query correctly.
  psm_rlwe::RlwePlaintextId psm_id_query_with_non_sensitive_slice =
      psm_id_query_day.value();
  psm_id_query_with_non_sensitive_slice.set_non_sensitive_id(
      window_id_query_day);

  return {psm_id_query_with_non_sensitive_slice};
}

void TwentyEightDayImpl::CheckMembershipOprfSecondPhase() {
  DCHECK(!url_loader_);

  if (!IsFirstPhaseComplete()) {
    NOTREACHED_IN_MIGRATION();
    std::move(callback_).Run();
    return;
  }
  PsmClientManager* psm_client_manager = GetParams()->GetPsmClientManager();
  psm_client_manager->SetPsmRlweClient(kPsmUseCase,
                                       GetPsmIdentifiersToQueryPhaseTwo());

  if (!psm_client_manager->GetPsmRlweClient()) {
    LOG(ERROR) << "Second phase of check membership failed since the PSM RLWE "
                  "client could "
               << "not be initialized.";
    std::move(callback_).Run();
    return;
  }

  // Generate PSM Oprf request body.
  const auto status_or_oprf_request = psm_client_manager->CreateOprfRequest();
  if (!status_or_oprf_request.ok()) {
    LOG(ERROR) << "Failed to create first phase OPRF request.";
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
      base::BindOnce(
          &TwentyEightDayImpl::OnCheckMembershipOprfCompleteSecondPhase,
          weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void TwentyEightDayImpl::OnCheckMembershipOprfCompleteSecondPhase(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::k28DA, utils::PsmRequest::kOprf,
                            net_code);

  // Convert serialized response body to oprf response protobuf.
  FresnelPsmRlweOprfResponse psm_oprf_response;
  bool is_response_body_set = response_body.get() != nullptr;

  if (!is_response_body_set ||
      !psm_oprf_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Second phase OPRF response net code = " << net_code;
    LOG(ERROR) << "Response body was not set or could not be parsed into "
               << "FresnelPsmRlweOprfResponse proto. "
               << "Is response body set = " << is_response_body_set;
    std::move(callback_).Run();
    return;
  }

  if (!psm_oprf_response.has_rlwe_oprf_response()) {
    LOG(ERROR) << "Second phase OPRF response net code = " << net_code;
    LOG(ERROR) << "FresnelPsmRlweOprfResponse is missing the actual oprf "
                  "response from server.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
      psm_oprf_response.rlwe_oprf_response();

  CheckMembershipQuerySecondPhase(oprf_response);
}

void TwentyEightDayImpl::CheckMembershipQuerySecondPhase(
    const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
        oprf_response) {
  DCHECK(!url_loader_);

  PsmClientManager* psm_client_manager = GetParams()->GetPsmClientManager();

  // Generate PSM Query request body.
  const auto status_or_query_request =
      psm_client_manager->CreateQueryRequest(oprf_response);
  if (!status_or_query_request.ok()) {
    LOG(ERROR) << "First phrase failed to create Query request.";
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
      base::BindOnce(
          &TwentyEightDayImpl::OnCheckMembershipQueryCompleteSecondPhase,
          weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void TwentyEightDayImpl::OnCheckMembershipQueryCompleteSecondPhase(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::k28DA, utils::PsmRequest::kQuery,
                            net_code);

  // Convert serialized response body to fresnel query response protobuf.
  FresnelPsmRlweQueryResponse psm_query_response;
  bool is_response_body_set = response_body.get() != nullptr;

  if (!is_response_body_set ||
      !psm_query_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Second phase query response net code = " << net_code;
    LOG(ERROR) << "Response body was not set or could not be parsed into "
               << "FresnelPsmRlweQueryResponse proto. "
               << "Is response body set = " << is_response_body_set;
    std::move(callback_).Run();
    return;
  }

  if (!psm_query_response.has_rlwe_query_response()) {
    LOG(ERROR) << "Second phase query response net code = " << net_code;
    LOG(ERROR) << "FresnelPsmRlweQueryResponse is missing the actual query "
                  "response from server.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryResponse query_response =
      psm_query_response.rlwe_query_response();

  PsmClientManager* psm_client_manager = GetParams()->GetPsmClientManager();
  auto status_or_response =
      psm_client_manager->ProcessQueryResponse(query_response);

  if (!status_or_response.ok()) {
    LOG(ERROR) << "Second phase failed to process query response.";
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
      status_or_response.value();

  if (rlwe_membership_responses.membership_responses_size() != 1) {
    LOG(ERROR) << "Second phase of check membership for 28-day-active should "
                  "do exactly 1 membership. Size = "
               << rlwe_membership_responses.membership_responses_size();
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::RlweMembershipResponses::MembershipResponseEntry
      membership_response = rlwe_membership_responses.membership_responses(0);

  psm_rlwe::RlwePlaintextId searched_psm_id =
      membership_response.plaintext_id();
  bool is_psm_id_member = membership_response.membership_response().is_member();

  // Update window id for the searched psm with the membership result.
  actives_cache_.Set(searched_psm_id.non_sensitive_id(), is_psm_id_member);
  SaveActivesCachePref();

  if (IsSecondPhaseComplete()) {
    // TODO(hirthanan): Finish implementation here.
    base::Time last_ping_ts = FindLeftMostKnownMembership() - base::Days(28);
    LOG(ERROR) << "Device pinged last on = " << last_ping_ts;
    SetLastPingTimestamp(last_ping_ts);
    CheckIn();
    return;
  }

  CheckMembershipOprfSecondPhase();
}

}  // namespace ash::report::device_metrics
