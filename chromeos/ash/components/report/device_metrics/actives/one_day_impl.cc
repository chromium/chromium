// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/actives/one_day_impl.h"

#include "base/metrics/histogram_functions.h"
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

constexpr psm_rlwe::RlweUseCase kPsmUseCase =
    psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY;

// UMA histogram names for recording if oprf response body was set.
const char kHistogramsIsPsm1DAOprfResponseBodySet[] =
    "Ash.Report.IsPsm1DAOprfResponseBodySet";

// UMA histogram names for recording if oprf response was parsed correctly.
const char kHistogramsIsPsm1DAOprfResponseParsedCorrectly[] =
    "Ash.Report.IsPsm1DAOprfResponseParsedCorrectly";

// UMA histogram names for recording if query response was positive or negative.
const char kHistogramsPsmQueryMembershipResult[] =
    "Ash.Report.PsmQueryMembershipResult";

}  // namespace

OneDayImpl::OneDayImpl(UseCaseParameters* params) : UseCase(params) {}

OneDayImpl::~OneDayImpl() = default;

void OneDayImpl::Run(base::OnceCallback<void()> callback) {
  callback_ = std::move(callback);

  if (!IsDevicePingRequired()) {
    utils::RecordIsDevicePingRequired(utils::PsmUseCase::k1DA, false);
    std::move(callback_).Run();
    return;
  }

  utils::RecordIsDevicePingRequired(utils::PsmUseCase::k1DA, true);

  // Perform check membership if the local state pref has default value.
  // This is done to avoid duplicate check in if the device pinged already.
  if (GetLastPingTimestamp() == base::Time::UnixEpoch() ||
      GetLastPingTimestamp() == base::Time()) {
    CheckMembershipOprf();
  } else {
    CheckIn();
  }
}

base::WeakPtr<OneDayImpl> OneDayImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void OneDayImpl::CheckMembershipOprf() {
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
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kCreateOprfRequestFailed);
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
      base::BindOnce(&OneDayImpl::OnCheckMembershipOprfComplete,
                     weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void OneDayImpl::OnCheckMembershipOprfComplete(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::k1DA, utils::PsmRequest::kOprf,
                            net_code);

  // Convert serialized response body to oprf response protobuf.
  FresnelPsmRlweOprfResponse psm_oprf_response;
  bool is_response_body_set = response_body.get() != nullptr;
  base::UmaHistogramBoolean(kHistogramsIsPsm1DAOprfResponseBodySet,
                            is_response_body_set);

  if (!is_response_body_set ||
      !psm_oprf_response.ParseFromString(*response_body)) {
    base::UmaHistogramBoolean(kHistogramsIsPsm1DAOprfResponseParsedCorrectly,
                              false);
    LOG(ERROR) << "Oprf response net code = " << net_code;
    LOG(ERROR) << "Response body was not set or could not be parsed into "
               << "FresnelPsmRlweOprfResponse proto. "
               << "Is response body set = " << is_response_body_set;
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kOprfResponseBodyFailed);
    std::move(callback_).Run();
    return;
  }

  base::UmaHistogramBoolean(kHistogramsIsPsm1DAOprfResponseParsedCorrectly,
                            true);

  if (!psm_oprf_response.has_rlwe_oprf_response()) {
    LOG(ERROR) << "Oprf response net code = " << net_code;
    LOG(ERROR) << "FresnelPsmRlweOprfResponse is missing the actual oprf "
                  "response from server.";
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kNotHasRlweOprfResponse);
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
      psm_oprf_response.rlwe_oprf_response();

  CheckMembershipQuery(oprf_response);
}

void OneDayImpl::CheckMembershipQuery(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  PsmClientManager* psm_client_manager = GetParams()->GetPsmClientManager();

  // Generate PSM Query request body.
  const auto status_or_query_request =
      psm_client_manager->CreateQueryRequest(oprf_response);
  if (!status_or_query_request.ok()) {
    LOG(ERROR) << "Failed to create Query request.";
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kCreateQueryRequestFailed);
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
      base::BindOnce(&OneDayImpl::OnCheckMembershipQueryComplete,
                     weak_factory_.GetWeakPtr()),
      utils::GetMaxFresnelResponseSizeBytes());
}

void OneDayImpl::OnCheckMembershipQueryComplete(
    std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::k1DA, utils::PsmRequest::kQuery,
                            net_code);

  // Convert serialized response body to fresnel query response protobuf.
  FresnelPsmRlweQueryResponse psm_query_response;
  bool is_response_body_set = response_body.get() != nullptr;

  if (!is_response_body_set ||
      !psm_query_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Query response net code = " << net_code;
    LOG(ERROR) << "Response body was not set or could not be parsed into "
               << "FresnelPsmRlweQueryResponse proto. "
               << "Is response body set = " << is_response_body_set;
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kQueryResponseBodyFailed);
    std::move(callback_).Run();
    return;
  }

  if (!psm_query_response.has_rlwe_query_response()) {
    LOG(ERROR) << "Query response net code = " << net_code;
    LOG(ERROR) << "FresnelPsmRlweQueryResponse is missing the actual query "
                  "response from server.";
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kNotHasRlweQueryResponse);
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryResponse query_response =
      psm_query_response.rlwe_query_response();
  auto status_or_response =
      GetParams()->GetPsmClientManager()->ProcessQueryResponse(query_response);

  if (!status_or_response.ok()) {
    LOG(ERROR) << "Failed to process query response.";
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kProcessQueryResponseFailed);
    std::move(callback_).Run();
    return;
  }

  psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
      status_or_response.value();

  if (rlwe_membership_responses.membership_responses_size() != 1) {
    LOG(ERROR) << "Check Membership for 1-day-active should only query for a "
                  "single response."
               << "Size = "
               << rlwe_membership_responses.membership_responses_size();
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kMembershipResponsesSizeIsNotOne);
    std::move(callback_).Run();
    return;
  }

  private_membership::MembershipResponse membership_response =
      rlwe_membership_responses.membership_responses(0).membership_response();

  bool is_psm_id_member = membership_response.is_member();
  base::UmaHistogramBoolean(kHistogramsPsmQueryMembershipResult,
                            is_psm_id_member);

  if (is_psm_id_member) {
    LOG(ERROR) << "Check in ping was already sent earlier today.";
    SetLastPingTimestamp(GetParams()->GetActiveTs());
    std::move(callback_).Run();
    return;
  }

  utils::RecordCheckMembershipCases(
      utils::PsmUseCase::k1DA,
      utils::CheckMembershipResponseCases::kIsNotPsmIdMember);
  CheckIn();
}

void OneDayImpl::CheckIn() {
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
                                base::BindOnce(&OneDayImpl::OnCheckInComplete,
                                               weak_factory_.GetWeakPtr()),
                                utils::GetMaxFresnelResponseSizeBytes());
}

void OneDayImpl::OnCheckInComplete(std::unique_ptr<std::string> response_body) {
  // Use RAII to reset |url_loader_| after current function scope.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  utils::RecordNetErrorCode(utils::PsmUseCase::k1DA, utils::PsmRequest::kImport,
                            net_code);

  if (net_code == net::OK) {
    // Update local state pref to record reporting device active.
    SetLastPingTimestamp(GetParams()->GetActiveTs());
    utils::RecordCheckMembershipCases(
        utils::PsmUseCase::k1DA,
        utils::CheckMembershipResponseCases::kSuccessfullySetLocalState);
  } else {
    LOG(ERROR) << "Failed to check in successfully. Net code = " << net_code;
  }

  // Check-in completed - use case is done running.
  std::move(callback_).Run();
}

base::Time OneDayImpl::GetLastPingTimestamp() {
  return GetParams()->GetLocalState()->GetTime(
      ash::report::prefs::kDeviceActiveLastKnown1DayActivePingTimestamp);
}

void OneDayImpl::SetLastPingTimestamp(base::Time ts) {
  GetParams()->GetLocalState()->SetTime(
      ash::report::prefs::kDeviceActiveLastKnown1DayActivePingTimestamp, ts);
}

std::vector<psm_rlwe::RlwePlaintextId> OneDayImpl::GetPsmIdentifiersToQuery() {
  std::string window_id =
      utils::TimeToYYYYMMDDString(GetParams()->GetActiveTs());
  std::optional<psm_rlwe::RlwePlaintextId> psm_id =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id);

  if (!psm_id.has_value()) {
    LOG(ERROR) << "Failed to generate PSM ID to query.";
    return {};
  }

  std::vector<psm_rlwe::RlwePlaintextId> query_psm_ids = {psm_id.value()};
  return query_psm_ids;
}

std::optional<FresnelImportDataRequest>
OneDayImpl::GenerateImportRequestBody() {
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

  std::string window_id =
      utils::TimeToYYYYMMDDString(GetParams()->GetActiveTs());
  std::optional<psm_rlwe::RlwePlaintextId> psm_id =
      utils::GeneratePsmIdentifier(GetParams()->GetHighEntropySeed(),
                                   psm_rlwe::RlweUseCase_Name(kPsmUseCase),
                                   window_id);

  if (!psm_id.has_value()) {
    return std::nullopt;
  }

  FresnelImportData* import_data = import_request.add_import_data();
  import_data->set_window_identifier(window_id);
  import_data->set_plaintext_id(psm_id.value().sensitive_id());
  import_data->set_is_pt_window_identifier(true);

  return import_request;
}

bool OneDayImpl::IsDevicePingRequired() {
  base::Time last_ping_ts = GetLastPingTimestamp();
  base::Time cur_ping_ts = GetParams()->GetActiveTs();

  std::string last_ping_day = utils::TimeToYYYYMMDDString(last_ping_ts);
  std::string cur_ping_day = utils::TimeToYYYYMMDDString(cur_ping_ts);

  // Safety check to avoid against clock drift, or unexpected timestamps.
  // Check should make sure that we are not reporting window id's for
  // day's previous to one that we reported already.
  return last_ping_ts < cur_ping_ts && last_ping_day != cur_ping_day;
}

}  // namespace ash::report::device_metrics
