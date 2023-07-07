// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_USE_CASE_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_USE_CASE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/report/proto/fresnel_service.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net
namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash::report::device_metrics {

// Fields used in setting device active metadata, that are explicitly
// required from outside of ash-chrome due to the dependency limitations
// on the chrome browser.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
    ChromeDeviceMetadataParameters {
  version_info::Channel chrome_channel;
  MarketSegment market_segment;
};

// Create a delegate which can be used to create fakes in unit tests.
// Fake via. delegate is required for creating deterministic unit tests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT) PsmDelegateInterface {
 public:
  virtual ~PsmDelegateInterface() = default;
  virtual rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(private_membership::rlwe::RlweUseCase use_case,
                  const std::vector<private_membership::rlwe::RlwePlaintextId>&
                      plaintext_ids) = 0;
};

// Helper class to group UseCase class parameters.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT) UseCaseParameters {
 public:
  UseCaseParameters(
      const base::Time active_ts,
      const ChromeDeviceMetadataParameters& chrome_device_params,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& high_entropy_seed,
      PrefService* local_state,
      std::unique_ptr<PsmDelegateInterface> psm_delegate);
  ~UseCaseParameters();

  const base::Time GetActiveTs() const;

  // Used to generate the device metadata sent in the import request.
  const ChromeDeviceMetadataParameters& GetChromeDeviceParams() const;

  // Used to create url loader objects, that send network requests to Fresnel.
  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() const;

  // Used to generate the pseudonymous id sent in the import request.
  const std::string& GetHighEntropySeed() const;

  // Get and set fresnel pref keys.
  PrefService* GetLocalState() const;

  // Used to enable passing the real and fake PSM client.
  PsmDelegateInterface* GetPsmDelegate() const;

 private:
  // Represents the device's online timestamp, adjusted to Pacific Time (PST).
  //
  // It is a `base::Time` object that is adjusted once during initialization and
  // then propagated across child classes use cases (1DA, 28DA, Churn, etc.).
  // base::Time methods will be PST-disguised-as-UTC-things (UTCExplode,
  // UTCMidnight calls).
  //
  // It is used to compare dates in the Pacific Time zone, determine
  // new PST days, store data in local and preserved file caches, and
  // provide day-level metadata in network requests to Fresnel.
  const base::Time active_ts_;

  // Get the device metadata parameters passed by chrome.
  const ChromeDeviceMetadataParameters chrome_device_params_;

  // Creates SimpleURLLoader objects, used to send network requests to Fresnel.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // High entropy seed used to generate pseudonymous ids when importing.
  const std::string high_entropy_seed_;

  // Persists fresnel pref key/value pairs over device restarts.
  const raw_ptr<PrefService, ExperimentalAsh> local_state_;

  // Abstract class used to generate the PSM RLWE client.
  std::unique_ptr<PsmDelegateInterface> psm_delegate_;
};

// Base class for each use case that is reporting to Fresnel server.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT) UseCase {
 public:
  explicit UseCase(UseCaseParameters* params);
  UseCase(const UseCase&) = delete;
  UseCase& operator=(const UseCase&) = delete;
  virtual ~UseCase();

  // Execute reporting flow to Fresnel Server, using private set membership.
  // Run the provided callback before returning from this method.
  virtual void Run(base::OnceCallback<void()> callback) = 0;

 protected:
  // First phase of PSM check membership.
  // Get the query params needed for the second phase.
  virtual void CheckMembershipOprf() = 0;

  // Handle the response from the Oprf request.
  virtual void OnCheckMembershipOprfComplete(
      std::unique_ptr<std::string> response_body) = 0;

  // Second phase of PSM check membership.
  // Get the result of whether the queried element(s) are in the set.
  virtual void CheckMembershipQuery(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response) = 0;

  // Handle the response from the Query request.
  virtual void OnCheckMembershipQueryComplete(
      std::unique_ptr<std::string> response_body) = 0;

  // Import data to the Fresnel, and PSM database backend.
  virtual void CheckIn() = 0;

  // Handle the response from the Check-in request.
  virtual void OnCheckInComplete(
      std::unique_ptr<std::string> response_body) = 0;

  // Retrieve the last known ping timestamp for the use case from local state.
  virtual base::Time GetLastPingTimestamp() = 0;

  // Sets the last ping timestamp in local state to the specified value.
  // This value is used to determine when the next ping should be sent.
  virtual void SetLastPingTimestamp(base::Time ts) = 0;

  // Get all the identifiers that the use case may have to query PSM for.
  virtual std::vector<private_membership::rlwe::RlwePlaintextId>
  GetPsmIdentifiersToQuery() = 0;

  // Create the import request body that is sent to Fresnel.
  // Important: Any dimension that is sent requires privacy approval.
  virtual absl::optional<FresnelImportDataRequest>
  GenerateImportRequestBody() = 0;

  // Define the Fresnel network request annotation tags.
  net::NetworkTrafficAnnotationTag GetCheckMembershipTrafficTag();
  net::NetworkTrafficAnnotationTag GetCheckInTrafficTag();

  // Return address of |psm_rlwe_client_| unique pointer, or null if not set.
  private_membership::rlwe::PrivateMembershipRlweClient* GetPsmRlweClient();

  // Generate the PSM RLWE client used to create OPRF and Query request bodies.
  void SetPsmRlweClient(
      private_membership::rlwe::RlweUseCase psm_use_case,
      std::vector<private_membership::rlwe::RlwePlaintextId> query_psm_ids);

  UseCaseParameters* GetParams() const;

 private:
  // Used to generate the request body of Oprf and Query requests.
  std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;

  // Store shared params that are use across all use cases.
  // Field will live throughout object lifetime.
  const raw_ptr<UseCaseParameters> params_;
};

}  // namespace ash::report::device_metrics

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_USE_CASE_H_
