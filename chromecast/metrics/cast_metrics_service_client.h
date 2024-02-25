// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_CAST_METRICS_SERVICE_CLIENT_H_
#define CHROMECAST_METRICS_CAST_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chromecast/public/cast_sys_info.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/persistent_synthetic_trial_observer.h"
#include "components/variations/synthetic_trial_registry.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class SingleThreadTaskRunner;
}

namespace metrics {
struct ClientInfo;
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

namespace network {
class SharedURLLoaderFactory;
}

namespace chromecast {
namespace metrics {

class CastMetricsServiceDelegate {
 public:
  // Invoked when the metrics client ID changes.
  virtual void SetMetricsClientId(const std::string& client_id) = 0;

  // Allows registration of extra metrics providers.
  virtual void RegisterMetricsProviders(::metrics::MetricsService* service) = 0;

  // Adds Cast embedder-specific storage limits to |limits| object.
  virtual void ApplyMetricsStorageLimits(
      ::metrics::MetricsLogStore::StorageLimits* limits) {}

 protected:
  virtual ~CastMetricsServiceDelegate() = default;
};

class CastMetricsServiceClient : public ::metrics::MetricsServiceClient,
                                 public ::metrics::EnabledStateProvider {
 public:
  CastMetricsServiceClient(
      CastMetricsServiceDelegate* delegate,
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~CastMetricsServiceClient() override;

  CastMetricsServiceClient(const CastMetricsServiceClient&) = delete;
  CastMetricsServiceClient& operator=(const CastMetricsServiceClient&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Use |client_id| when starting MetricsService instead of generating a new
  // client ID. If used, SetForceClientId must be called before Initialize.
  void SetForceClientId(const std::string& client_id);
  void OnApplicationNotIdle();

  // Processes all events from shared file. This should be used to consume all
  // events in the file before shutdown. This function is safe to call from any
  // thread.
  void ProcessExternalEvents(base::OnceClosure cb);

  void InitializeMetricsService();
  void StartMetricsService();
  void Finalize();

  // ::metrics::MetricsServiceClient:
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override;
  ::metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool GetBrand(std::string* brand_code) override;
  ::metrics::SystemProfileProto::Channel GetChannel() override;
  bool IsExtendedStableChannel() override;
  std::string GetVersionString() override;
  void CollectFinalMetricsForLog(base::OnceClosure done_callback) override;
  GURL GetMetricsServerUrl() override;
  std::unique_ptr<::metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      std::string_view mime_type,
      ::metrics::MetricsLogUploader::MetricServiceType service_type,
      const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  ::metrics::MetricsLogStore::StorageLimits GetStorageLimits() const override;

  // ::metrics::EnabledStateProvider:
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;

  // Starts/stops the metrics service.
  void UpdateMetricsServiceState();
  void DisableMetricsService();

  std::string client_id() const { return client_id_; }

  PrefService* pref_service() const { return pref_service_; }
  void SetCallbacks(
      base::RepeatingCallback<void(base::OnceClosure)> collect_final_metrics_cb,
      base::RepeatingCallback<void(base::OnceClosure)> external_events_cb);

 private:
  std::unique_ptr<::metrics::ClientInfo> LoadClientInfo();
  void StoreClientInfo(const ::metrics::ClientInfo& client_info);

  CastMetricsServiceDelegate* const delegate_;
  PrefService* const pref_service_;
  std::string client_id_;
  std::string force_client_id_;
  bool client_info_loaded_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<::metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
  ::metrics::PersistentSyntheticTrialObserver synthetic_trial_observer_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_observation_{&synthetic_trial_observer_};
  std::unique_ptr<::metrics::MetricsService> metrics_service_;
  std::unique_ptr<::metrics::EnabledStateProvider> enabled_state_provider_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::RepeatingCallback<void(base::OnceClosure)> collect_final_metrics_cb_;
  base::RepeatingCallback<void(base::OnceClosure)> external_events_cb_;
  const std::unique_ptr<CastSysInfo> cast_sys_info_;
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_METRICS_CAST_METRICS_SERVICE_CLIENT_H_
