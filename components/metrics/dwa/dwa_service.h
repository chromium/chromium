// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_SERVICE_H_
#define COMPONENTS_METRICS_DWA_DWA_SERVICE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/metrics_rotation_scheduler.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/private_metrics/data_upload_config_downloader.h"
#include "components/metrics/private_metrics/private_metrics_reporting_service.h"
#include "components/metrics/unsent_log_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/cose.h"
#include "third_party/federated_compute/src/fcp/protos/confidentialcompute/data_upload_config.pb.h"
#include "third_party/metrics_proto/dwa/deidentified_web_analytics.pb.h"
#include "third_party/metrics_proto/private_metrics/private_metrics.pb.h"

namespace metrics::dwa {

// The DwaService is responsible for collecting and uploading deindentified web
// analytics events.
class DwaService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the encryption public key is changed.
    virtual void OnEncryptionPublicKeyChanged(
        const fcp::confidential_compute::OkpCwt& decoded_public_key) = 0;
  };

  DwaService(MetricsServiceClient* client,
             PrefService* pref_service,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  DwaService(const DwaService&) = delete;
  DwaService& operator=(const DwaService&) = delete;

  ~DwaService();

  void EnableReporting();
  void DisableReporting();

  // Flushes any event currently in the recorder to prefs.
  void Flush(metrics::MetricsLogsEventManager::CreateReason reason);

  // Clears all event and log data.
  void Purge();

  // Adds an observer to be notified of changes to the encryption public key.
  void AddObserver(Observer* observer);

  // Removes an observer to be notified of changes to the encryption public key.
  void RemoveObserver(Observer* observer);

  // Gets the decoded encryption public key being used to encrypt private metric
  // reports. If the key is invalid or not available, returns an empty optional.
  std::optional<fcp::confidential_compute::OkpCwt> GetEncryptionPublicKey();

  // Refresh the public key used to encrypt private metric reports.
  void RefreshEncryptionPublicKey();

  // Sets encryption public key used to encrypt private metrics reports as
  // `test_encryption_public_key`.
  void SetEncryptionPublicKeyForTesting(
      std::string_view test_encryption_public_key);

  // Sets the value of encryption public key verifier,
  // `encryption_public_key_verifier_`, to
  // `test_encryption_public_key_verifier`.
  // TODO(crbug.com/444679397): Remove this function and instead make
  // ValidateEncryptionPublicKey() a virtual function that can be overridden in
  // unit tests.
  void SetEncryptionPublicKeyVerifierForTesting(
      const base::RepeatingCallback<
          bool(const fcp::confidential_compute::OkpCwt&)>&
          test_encryption_public_key_verifier);

  // Records coarse system profile into CoarseSystemInfo of the deidentified web
  // analytics report proto.
  static void RecordCoarseSystemInformation(
      MetricsServiceClient& client,
      const PrefService& local_state,
      ::dwa::CoarseSystemInfo* coarse_system_info);

  // Generate client id which changes between days. We store this id in a
  // uint64 instead of base::Uuid as it is eventually stored in a proto with
  // this type. We are not concerned with id collisions as ids are only meant to
  // be compared within single days and they are used for k-anonymity (where it
  // would mean undercounting for k-anonymity).
  static uint64_t GetEphemeralClientId(PrefService& local_state);

  // Computes a persistent hash for the given `coarse_system_info`.
  static uint64_t HashCoarseSystemInfo(
      const ::dwa::CoarseSystemInfo& coarse_system_info);

  // Computes a persistent hash for a repeated list of field trials names and
  // groups. An empty optional is returned if `repeated_field_trials` cannot be
  // serialized into a value.
  static std::optional<uint64_t> HashRepeatedFieldTrials(
      const google::protobuf::RepeatedPtrField<
          ::metrics::SystemProfileProto::FieldTrial>& repeated_field_trials);

  // Builds the k-anonymity buckets for the `k_anonymity_buckets` field in
  // PrivateMetricReport protocol buffer. Each event may contain multiple
  // buckets that need to pass the k-anonymity filter. Buckets may contain
  // quasi-identifiers. We treat the k-anonymity bucket
  // values as opaque and do not attempt to interpret them. An empty vector is
  // returned and dropped from being reported if there is an error in building
  // k-anonymity buckets for `dwa_event` as there would be no way to enforce the
  // k-anonymity filter without the k-anonymity buckets. For `dwa_event`, the
  // combination of `dwa_event.coarse_system_info`, `dwa_event.event_hash`, and
  // `dwa_event.field_trials` builds the first k-anonymity bucket because the
  // combination describes an user invoking an action. We want to verify there
  // is a sufficient number of users who perform this action before allowing the
  // `dwa_event` past the k-anonymity filter. Similarly,
  // `dwa_event.content_metrics.content_hash` builds the second k-anonymity
  // bucket because we want to confirm that the subresource's eTLD+1 is a domain
  // with which a substantial number of users have interacted with.
  static std::vector<uint64_t> BuildKAnonymityBuckets(
      const ::dwa::DeidentifiedWebAnalyticsEvent& dwa_event);

  // Encrypts `report` using the public encryption key, `public_key`, which is
  // accepted by the HPKE encryption method in //third_party/federated_compute.
  // The encrypted private metrics report can only be decrypted in a trusted
  // execution environment (TEE) because decryption keys are released
  // execlusively to the TEE. This method accepts the field `decoded_public_key`
  // as an optimization. The field `decoded_public_key` is the OkpCwt
  // representation of `public_key`, which is parsed and validated before being
  // passed to this method. This prevents the need to re-parse the public key to
  // extract the key id, which is required when building the encrypted private
  // metrics report.
  static std::optional<::private_metrics::EncryptedPrivateMetricReport>
  EncryptPrivateMetricReport(
      const ::private_metrics::PrivateMetricReport& report,
      std::string_view public_key,
      const fcp::confidential_compute::OkpCwt& decoded_public_key);

  // Builds a PrivateMetricEndpointPayload from an
  // EncryptedPrivateMetricReport. This function is used to wrap the encrypted
  // DWAs in a PrivateMetricEndpointPayload before uploading to the Private
  // Metrics Collector (PMC) endpoint. The param is passed by value using
  // std::move() to avoid a copy. An empty optional is returned if the
  // report type is invalid.
  static std::optional<::private_metrics::PrivateMetricEndpointPayload>
  BuildPrivateMetricEndpointPayloadFromEncryptedReport(
      ::private_metrics::EncryptedPrivateMetricReport encrypted_report);

  // Returns false if the public key `cwt` is expired or should not be used.
  // Otherwise, returns true.
  static bool ValidateEncryptionPublicKey(
      const fcp::confidential_compute::OkpCwt& cwt);

  // Register prefs from `dwa_pref_names.h`.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  metrics::UnsentLogStore* unsent_log_store();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Periodically called by |scheduler_| to advance processing of logs.
  void RotateLog();

  // Constructs a new DeidentifiedWebAnalyticsReport from available data and
  // stores it in |unsent_log_store_|.
  void BuildDwaReportAndStoreLog(
      metrics::MetricsLogsEventManager::CreateReason reason);

  // Constructs a new PrivateMetricReport from available data and
  // stores it in `unsent_log_store_`.
  void BuildPrivateMetricReportAndStoreLog(
      metrics::MetricsLogsEventManager::CreateReason reason);

  // Retrieves the storage parameters to control the reporting service.
  static UnsentLogStore::UnsentLogStoreLimits GetLogStoreLimits();

  // Handles updating the public key, `encryption_public_key_`, during a public
  // key refresh.
  void HandleEncryptionPublicKeyRefresh(
      std::optional<fcp::confidentialcompute::DataUploadConfig>
          maybe_data_upload_config);

  // Returns true if the decoded public key is valid. Otherwise, returns false.
  bool IsValidCwt(const fcp::confidential_compute::OkpCwt& cwt);

  // Manages on-device recording of events.
  raw_ptr<DwaRecorder> recorder_;

  // The metrics client |this| is service is associated.
  raw_ptr<MetricsServiceClient> client_;

  // A weak pointer to the PrefService used to read and write preferences.
  raw_ptr<PrefService> pref_service_;

  // Service for uploading serialized logs to Private Metrics endpoint.
  private_metrics::PrivateMetricsReportingService reporting_service_;

  // The scheduler for determining when uploads should happen.
  std::unique_ptr<MetricsRotationScheduler> scheduler_;

  // The DataUploadConfig protocol buffer contains the public key required to
  // encrypt private metric reports and the signed endorsements of the keys
  // required to perform attestation verification.
  std::unique_ptr<private_metrics::DataUploadConfigDownloader>
      data_upload_config_downloader_;

  // The public key used to encrypt PrivateMetricReport protocol buffer. The
  // public key is fetched on startup and refreshed once every 24 hours.
  std::string encryption_public_key_;

  // A repeating callback used to verify the `encryption_public_key_`. The
  // callback should return false if the public key is invalid, expired, or
  // should not be used.
  // Otherwise, the callback should return true.
  base::RepeatingCallback<bool(const fcp::confidential_compute::OkpCwt&)>
      encryption_public_key_verifier_;

  // List of observers to be notified of changes to the encryption public key.
  base::ObserverList<Observer> observers_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as DwaService.
  base::WeakPtrFactory<DwaService> self_ptr_factory_{this};
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_SERVICE_H_
