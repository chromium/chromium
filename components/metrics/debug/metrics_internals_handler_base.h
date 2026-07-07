// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEBUG_METRICS_INTERNALS_HANDLER_BASE_H_
#define COMPONENTS_METRICS_DEBUG_METRICS_INTERNALS_HANDLER_BASE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/metrics/dwa/dwa_service.h"
#include "components/metrics/metrics_service_observer.h"
#include "components/variations/variations_seed_store.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/cose.h"

namespace metrics_services_manager {
class MetricsServicesManager;
}  // namespace metrics_services_manager

namespace metrics {

class MetricsService;

// Platform-agnostic logic for chrome://metrics-internals.
class MetricsInternalsHandlerBase : public dwa::DwaService::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void ResolvePageCallback(const base::ValueView callback_id,
                                     const base::ValueView response) = 0;

    virtual void FireWebUIListener(std::string_view event_name) = 0;
    virtual void FireWebUIListener(std::string_view event_name,
                                   const base::ValueView arg1) = 0;
  };

  MetricsInternalsHandlerBase(Delegate* delegate,
                              MetricsService* metrics_service,
                              metrics_services_manager::MetricsServicesManager*
                                  metrics_services_manager);

  MetricsInternalsHandlerBase(const MetricsInternalsHandlerBase&) = delete;
  MetricsInternalsHandlerBase& operator=(const MetricsInternalsHandlerBase&) =
      delete;

  ~MetricsInternalsHandlerBase() override;

  void StartObserving();
  void StopObserving();

  void HandleFetchVariationsSummary(const base::Value& callback_id);
  void HandleFetchStoredSeedInfo(
      variations::VariationsSeedStore::SeedType seed_type,
      const base::Value& callback_id);
  void HandleFetchUmaSummary(const base::Value& callback_id);
  void HandleFetchUmaLogsData(const base::Value& callback_id,
                              bool include_log_proto_data);
  void HandleFetchEncryptionPublicKey(const base::Value& callback_id);
  void HandleIsUsingMetricsServiceObserver(const base::Value& callback_id);

  bool ShouldUseMetricsServiceObserver();

  // dwa::DwaService::Observer:
  void OnEncryptionPublicKeyChanged(
      const fcp::confidential_compute::OkpCwt& decoded_key) override;

 private:
  metrics::MetricsServiceObserver* GetUmaObserver();
  void OnUmaLogCreatedOrEvent();

  void ResolveJsCallbackHelper(base::Value callback_id, base::ValueView result);

  const raw_ptr<Delegate> delegate_;
  const raw_ptr<MetricsService> metrics_service_;
  const raw_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  // This UMA log observer keeps track of logs since its creation. It is unused
  // if the UMA metrics service has its own observer that has observed all
  // events since browser startup.
  std::unique_ptr<metrics::MetricsServiceObserver> uma_log_observer_;

  // The callback subscription to |uma_log_observer_| that notifies the WebUI
  // of changes. When this subscription is destroyed, it is automatically
  // de-registered from the callback list.
  base::CallbackListSubscription uma_log_notified_subscription_;

  base::ScopedObservation<dwa::DwaService, dwa::DwaService::Observer>
      dwa_service_observation_{this};

  base::WeakPtrFactory<MetricsInternalsHandlerBase> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEBUG_METRICS_INTERNALS_HANDLER_BASE_H_
