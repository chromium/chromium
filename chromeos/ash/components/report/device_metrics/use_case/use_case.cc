// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"

#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

UseCaseParameters::UseCaseParameters(
    const base::Time active_ts,
    const ChromeDeviceMetadataParameters& chrome_device_params,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& high_entropy_seed,
    PrefService* local_state,
    std::unique_ptr<PsmDelegateInterface> psm_delegate)
    : active_ts_(active_ts),
      chrome_device_params_(chrome_device_params),
      url_loader_factory_(url_loader_factory),
      high_entropy_seed_(high_entropy_seed),
      local_state_(local_state),
      psm_delegate_(std::move(psm_delegate)) {}

UseCaseParameters::~UseCaseParameters() = default;

const base::Time UseCaseParameters::GetActiveTs() const {
  return active_ts_;
}

const ChromeDeviceMetadataParameters& UseCaseParameters::GetChromeDeviceParams()
    const {
  return chrome_device_params_;
}

scoped_refptr<network::SharedURLLoaderFactory>
UseCaseParameters::GetUrlLoaderFactory() const {
  return url_loader_factory_;
}

const std::string& UseCaseParameters::GetHighEntropySeed() const {
  return high_entropy_seed_;
}

PrefService* UseCaseParameters::GetLocalState() const {
  return local_state_;
}

PsmDelegateInterface* UseCaseParameters::GetPsmDelegate() const {
  return psm_delegate_.get();
}

UseCase::UseCase(UseCaseParameters* params) : params_(params) {}

UseCase::~UseCase() = default;

net::NetworkTrafficAnnotationTag UseCase::GetCheckMembershipTrafficTag() {
  return net::DefineNetworkTrafficAnnotation("report_check_membership",
                                             R"(
        semantics {
          sender: "Fresnel Report"
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
}

net::NetworkTrafficAnnotationTag UseCase::GetCheckInTrafficTag() {
  return net::DefineNetworkTrafficAnnotation("report_check_in", R"(
        semantics {
          sender: "Fresnel Report"
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
}

psm_rlwe::PrivateMembershipRlweClient* UseCase::GetPsmRlweClient() {
  return psm_rlwe_client_.get();
}

void UseCase::SetPsmRlweClient(
    psm_rlwe::RlweUseCase psm_use_case,
    std::vector<psm_rlwe::RlwePlaintextId> query_psm_ids) {
  DCHECK(!query_psm_ids.empty());

  auto status_or_client = GetParams()->GetPsmDelegate()->CreatePsmClient(
      psm_use_case, query_psm_ids);

  if (!status_or_client.ok()) {
    LOG(ERROR) << "Failed to initialize PSM RLWE client.";
    return;
  }

  // Re-assigning the unique_ptr will reset the old unique_ptr.
  psm_rlwe_client_ = std::move(status_or_client.value());
}

UseCaseParameters* UseCase::GetParams() const {
  return params_;
}

}  // namespace ash::report::device_metrics
