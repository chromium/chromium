// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"

#include "chromeos/ash/components/report/device_metrics/use_case/psm_client_manager.h"
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
    PsmClientManager* psm_client_manager)
    : active_ts_(active_ts),
      chrome_device_params_(chrome_device_params),
      url_loader_factory_(url_loader_factory),
      high_entropy_seed_(high_entropy_seed),
      local_state_(local_state),
      psm_client_manager_(psm_client_manager) {}

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

PsmClientManager* UseCaseParameters::GetPsmClientManager() const {
  return psm_client_manager_;
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
          internal {
            contacts {
              email: "hirthanan@google.com"
            }
            contacts {
              email: "chromeos-data-eng@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2023-07-05"
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
                  "Set of that LAN. The import request contains an encrypted "
                  "derived device id, and metadata used to calculate churn."
          trigger: "Chrome OS client makes this network request and records "
                   "the device activity when the default network changes"
          data: "Google API Key and metadata that is used to count device actives and churn."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "hirthanan@google.com"
            }
            contacts {
              email: "chromeos-data-eng@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: OTHER
          }
          last_reviewed: "2023-07-05"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Not implemented."
        })");
}

UseCaseParameters* UseCase::GetParams() const {
  return params_;
}

}  // namespace ash::report::device_metrics
