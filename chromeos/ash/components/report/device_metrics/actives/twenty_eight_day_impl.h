// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_ACTIVES_TWENTY_EIGHT_DAY_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_ACTIVES_TWENTY_EIGHT_DAY_IMPL_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ash::report::device_metrics {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT) TwentyEightDayImpl
    : public UseCase {
 public:
  explicit TwentyEightDayImpl(UseCaseParameters* params);
  TwentyEightDayImpl(const TwentyEightDayImpl&) = delete;
  TwentyEightDayImpl& operator=(const TwentyEightDayImpl&) = delete;
  ~TwentyEightDayImpl() override;

  // UseCase:
  void Run(base::OnceCallback<void()> callback) override;

  // Used by ReportController to destruct pending callbacks appropriately.
  base::WeakPtr<TwentyEightDayImpl> GetWeakPtr();

 protected:
  // UseCase:
  void CheckMembershipOprf() override;
  void OnCheckMembershipOprfComplete(
      std::unique_ptr<std::string> response_body) override;
  void CheckMembershipQuery(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response) override;
  void OnCheckMembershipQueryComplete(
      std::unique_ptr<std::string> response_body) override;
  void CheckIn() override;

  // 28DA use case passes the import request back to callback completion.
  void OnCheckInComplete(std::unique_ptr<std::string> response_body) override;
  void OnCheckInCompleteCustom(const FresnelImportDataRequest import_request,
                               std::unique_ptr<std::string> response_body);

  base::Time GetLastPingTimestamp() override;
  void SetLastPingTimestamp(base::Time ts) override;
  std::vector<private_membership::rlwe::RlwePlaintextId>
  GetPsmIdentifiersToQuery() override;
  std::optional<FresnelImportDataRequest> GenerateImportRequestBody() override;

 private:
  // Grant friend access for comprehensive testing of private/protected members.
  friend class TwentyEightDayImplBase;
  friend class TwentyEightDayImplDirectCheckIn;
  friend class TwentyEightDayImplDirectCheckMembership;

  // Check if the device needs to ping today.
  bool IsDevicePingRequired();

  // Load the 28 day actives cache ping history of the device.
  void LoadActivesCachePref();

  // Save the 28 day actives cache ping history of the device.
  // We call this method every time actives cache is updated to avoid retrying
  // across unexpected restarts and crashes.
  void SaveActivesCachePref();

  // Recalculate the |actives_cache_| to clear any old entries.
  void FilterActivesCache();

  // Find leftmost known membership in |actives_cache_|,
  // used in second phase.
  base::Time FindLeftMostKnownMembership();

  // Find rightmost known non-membership in |actives_cache_|,
  // used in second phase.
  base::Time FindRightMostKnownNonMembership();

  // First phase of check membership should be for day 0, 1, and 27.
  bool IsFirstPhaseComplete();
  std::vector<private_membership::rlwe::RlwePlaintextId>
  GetPsmIdentifiersToQueryPhaseOne();
  void CheckMembershipOprfFirstPhase();
  void OnCheckMembershipOprfCompleteFirstPhase(
      std::unique_ptr<std::string> response_body);
  void CheckMembershipQueryFirstPhase(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response);
  void OnCheckMembershipQueryCompleteFirstPhase(
      std::unique_ptr<std::string> response_body);

  // Second phase of check membership should binary search for a single
  // identifier between day 2 and 26.
  bool IsSecondPhaseComplete();
  std::vector<private_membership::rlwe::RlwePlaintextId>
  GetPsmIdentifiersToQueryPhaseTwo();
  void CheckMembershipOprfSecondPhase();
  void OnCheckMembershipOprfCompleteSecondPhase(
      std::unique_ptr<std::string> response_body);
  void CheckMembershipQuerySecondPhase(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response);
  void OnCheckMembershipQueryCompleteSecondPhase(
      std::unique_ptr<std::string> response_body);

  // Maintains callback that is executed once this use case is done running.
  base::OnceCallback<void()> callback_;

  // Manage Oprf, Query, and Import network requests on a single sequence.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Pref to store the rolling history of 28 day actives.
  base::Value::Dict actives_cache_;

  // Automatically cancels callbacks when the referent of weakptr gets
  // destroyed.
  base::WeakPtrFactory<TwentyEightDayImpl> weak_factory_{this};
};

}  // namespace ash::report::device_metrics

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_ACTIVES_TWENTY_EIGHT_DAY_IMPL_H_
