// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_ACTIVES_ONE_DAY_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_ACTIVES_ONE_DAY_IMPL_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ash::report::device_metrics {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT) OneDayImpl
    : public UseCase {
 public:
  explicit OneDayImpl(UseCaseParameters* params);
  OneDayImpl(const OneDayImpl&) = delete;
  OneDayImpl& operator=(const OneDayImpl&) = delete;
  ~OneDayImpl() override;

  // UseCase:
  void Run(base::OnceCallback<void()> callback) override;

  // Used by ReportController to destruct pending callbacks appropriately.
  base::WeakPtr<OneDayImpl> GetWeakPtr();

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
  void OnCheckInComplete(std::unique_ptr<std::string> response_body) override;
  base::Time GetLastPingTimestamp() override;
  void SetLastPingTimestamp(base::Time ts) override;
  std::vector<private_membership::rlwe::RlwePlaintextId>
  GetPsmIdentifiersToQuery() override;
  std::optional<FresnelImportDataRequest> GenerateImportRequestBody() override;

 private:
  // Grant friend access for comprehensive testing of private/protected members.
  friend class OneDayImplBase;
  friend class OneDayImplWithPsmQueryPositive;
  friend class OneDayImplWithPsmQueryNegative;

  // Check if the device needs to ping today.
  bool IsDevicePingRequired();

  // Maintains callback that is executed once this use case is done running.
  base::OnceCallback<void()> callback_;

  // Manage Oprf, Query, and Import network requests on a single sequence.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Automatically cancels callbacks when the referent of weakptr gets
  // destroyed.
  base::WeakPtrFactory<OneDayImpl> weak_factory_{this};
};

}  // namespace ash::report::device_metrics

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_ACTIVES_ONE_DAY_IMPL_H_
