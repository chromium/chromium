// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/network/metrics/connection_info_metrics_logger.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

class NetworkMetadataStore;

// Provides APIs for logging metrics related to cellular networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularNetworkMetricsLogger
    : public ConnectionInfoMetricsLogger::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ApnTypes {
    kDefault = 0,
    kAttach = 1,
    kDefaultAndAttach = 2,
    kMaxValue = kDefaultAndAttach
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UnmanagedApnMigrationType {
    kMatchesLastGoodApn = 0,
    kDoesNotMatchLastGoodApn = 1,
    kMatchesLastConnectedAttachAndDefault = 2,
    kMatchesLastConnectedAttachHasMatchingDatabaseApn = 3,    // deprecated
    kMatchesLastConnectedAttachHasNoMatchingDatabaseApn = 4,  // deprecated
    kMatchesLastConnectedDefaultNoLastConnectedAttach = 5,
    kNoMatchingConnectedApn = 6,
    kMatchesLastConnectedAttachOnlyAndDefaultExists = 7,
    kMatchesLastConnectedDefaultOnlyAndAttachExists = 8,
    kMaxValue = kMatchesLastConnectedDefaultOnlyAndAttachExists
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ManagedApnMigrationType {
    kMatchesSelectedApn = 0,
    kDoesNotMatchSelectedApn = 1,
    kMaxValue = kDoesNotMatchSelectedApn
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ESimOperationResult {
    kSuccess = 0,
    kInhibitFailed = 1,
    kHermesFailed = 2,
    kMaxValue = kHermesFailed,
  };

  // These values are not persisted to logs and are used as helper values for
  // identifying the trigger of an SM-DS scan.
  enum class SmdsScanMethod {
    kViaPolicy = 0,
    kViaUser = 1,
    kMaxValue = kViaUser,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ESimUserInstallMethod {
    kViaSmds = 0,
    kViaQrCodeAfterSmds = 1,
    kViaQrCodeSkippedSmds = 2,
    kViaActivationCodeAfterSmds = 3,
    kViaActivationCodeSkippedSmds = 4,
    kMaxValue = kViaActivationCodeSkippedSmds,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ESimPolicyInstallMethod {
    kViaSmdp = 0,
    kViaSmds = 1,
    kMaxValue = kViaSmds,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UserTextMessageSuppressionState {
    kTextMessagesAllow = 0,
    kTextMessagesSuppress = 1,
    kMaxValue = kTextMessagesSuppress,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PolicyTextMessageSuppressionState {
    kUnset = 0,
    kTextMessagesAllow = 1,
    kTextMessagesSuppress = 2,
    kMaxValue = kTextMessagesSuppress,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NotificationSuppressionState {
    kNotSuppressed = 0,
    kUserSuppressed = 1,
    kPolicySuppressed = 2,
    kMaxValue = kPolicySuppressed,
  };

  static constexpr char kCreateCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.CreateCustomApn.Result";
  static constexpr char kCreateCustomApnAuthenticationTypeHistogram[] =
      "Network.Ash.Cellular.Apn.CreateCustomApn.AuthenticationType";
  static constexpr char kCreateCustomApnIpTypeHistogram[] =
      "Network.Ash.Cellular.Apn.CreateCustomApn.IpType";
  static constexpr char kCreateCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.CreateCustomApn.ApnTypes";
  static constexpr char kCreateExclusivelyEnabledCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.CreateExclusivelyEnabledCustomApn.Result";
  static constexpr char
      kCreateExclusivelyEnabledCustomApnAuthenticationTypeHistogram[] =
          "Network.Ash.Cellular.Apn.CreateExclusivelyEnabledCustomApn."
          "AuthenticationType";
  static constexpr char kCreateExclusivelyEnabledCustomApnIpTypeHistogram[] =
      "Network.Ash.Cellular.Apn.CreateExclusivelyEnabledCustomApn.IpType";
  static constexpr char kCreateExclusivelyEnabledCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.CreateExclusivelyEnabledCustomApn.ApnTypes";
  static constexpr char kRemoveCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.RemoveCustomApn.Result";
  static constexpr char kRemoveCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.RemoveCustomApn.ApnTypes";
  static constexpr char kModifyCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.ModifyCustomApn.Result";
  static constexpr char kModifyCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.ModifyCustomApn.ApnTypes";
  static constexpr char kEnableCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.EnableCustomApn.Result";
  static constexpr char kEnableCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.EnableCustomApn.ApnTypes";
  static constexpr char kDisableCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.DisableCustomApn.Result";
  static constexpr char kDisableCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.DisableCustomApn.ApnTypes";
  static constexpr char kConnectResultHasEnabledCustomApnsAllHistogram[] =
      "Network.Ash.Cellular.ConnectionResult.HasEnabledCustomApns.All";
  static constexpr char kConnectResultNoEnabledCustomApnsAllHistogram[] =
      "Network.Ash.Cellular.ConnectionResult.NoEnabledCustomApns.All";
  static constexpr char kCustomApnsCountHistogram[] =
      "Network.Ash.Cellular.Apn.CustomApns.Count";
  static constexpr char kCustomApnsEnabledCountHistogram[] =
      "Network.Ash.Cellular.Apn.CustomApns.Enabled.Count";
  static constexpr char kCustomApnsDisabledCountHistogram[] =
      "Network.Ash.Cellular.Apn.CustomApns.Disabled.Count";
  static constexpr char kCustomApnsUnmanagedMigrationTypeHistogram[] =
      "Network.Ash.Cellular.Apn.Unmanaged.MigrationType";
  static constexpr char kCustomApnsManagedMigrationTypeHistogram[] =
      "Network.Ash.Cellular.Apn.Managed.MigrationType";

  static constexpr char kSmdsScanViaPolicyProfileCount[] =
      "Network.Ash.Cellular.ESim.SmdsScan.ViaPolicy.ProfileCount";
  static constexpr char kSmdsScanViaUserProfileCount[] =
      "Network.Ash.Cellular.ESim.SmdsScan.ViaUser.ProfileCount";
  static constexpr char kSmdsScanOtherDurationSuccess[] =
      "Network.Ash.Cellular.ESim.SmdsScanDuration2.Other.OnSuccess";
  static constexpr char kSmdsScanOtherDurationFailure[] =
      "Network.Ash.Cellular.ESim.SmdsScanDuration2.Other.OnFailure";
  static constexpr char kSmdsScanAndroidDurationSuccess[] =
      "Network.Ash.Cellular.ESim.SmdsScanDuration2.Android.OnSuccess";
  static constexpr char kSmdsScanAndroidDurationFailure[] =
      "Network.Ash.Cellular.ESim.SmdsScanDuration2.Android.OnFailure";
  static constexpr char kSmdsScanGsmaDurationSuccess[] =
      "Network.Ash.Cellular.ESim.SmdsScanDuration2.Gsma.OnSuccess";
  static constexpr char kSmdsScanGsmaDurationFailure[] =
      "Network.Ash.Cellular.ESim.SmdsScanDuration2.Gsma.OnFailure";
  static constexpr char kESimUserInstallMethod[] =
      "Network.Ash.Cellular.ESim.UserInstall.Method";
  static constexpr char kESimPolicyInstallMethod[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.Method";
  static constexpr char kESimPolicyInstallNoAvailableProfiles[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.NoAvailableProfiles";
  static constexpr char kESimPolicyInstallUserErrorsFilteredAll[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsFiltered.All";
  static constexpr char kESimPolicyInstallUserErrorsFilteredViaSmdpInitial[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsFiltered."
      "ViaSmdpInitial";
  static constexpr char kESimPolicyInstallUserErrorsFilteredViaSmdpRetry[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsFiltered."
      "ViaSmdpRetry";
  static constexpr char kESimPolicyInstallUserErrorsFilteredViaSmdsInitial[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsFiltered."
      "ViaSmdsInitial";
  static constexpr char kESimPolicyInstallUserErrorsFilteredViaSmdsRetry[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsFiltered."
      "ViaSmdsRetry";
  static constexpr char kESimPolicyInstallUserErrorsIncludedAll[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsIncluded.All";
  static constexpr char kESimPolicyInstallUserErrorsIncludedViaSmdpInitial[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsIncluded."
      "ViaSmdpInitial";
  static constexpr char kESimPolicyInstallUserErrorsIncludedViaSmdpRetry[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsIncluded."
      "ViaSmdpRetry";
  static constexpr char kESimPolicyInstallUserErrorsIncludedViaSmdsInitial[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsIncluded."
      "ViaSmdsInitial";
  static constexpr char kESimPolicyInstallUserErrorsIncludedViaSmdsRetry[] =
      "Network.Ash.Cellular.ESim.PolicyInstall.UserErrorsIncluded."
      "ViaSmdsRetry";
  static constexpr char kESimUserInstallUserErrorsFilteredAll[] =
      "Network.Ash.Cellular.ESim.UserInstall.UserErrorsFiltered.All";
  static constexpr char
      kESimUserInstallUserErrorsFilteredViaActivationCodeAfterSmds[] =
          "Network.Ash.Cellular.ESim.UserInstall.UserErrorsFiltered."
          "ViaActivationCodeAfterSmds";
  static constexpr char
      kESimUserInstallUserErrorsFilteredViaActivationCodeSkippedSmds[] =
          "Network.Ash.Cellular.ESim.UserInstall.UserErrorsFiltered."
          "ViaActivationCodeSkippedSmds";
  static constexpr char kESimUserInstallUserErrorsFilteredViaQrCodeAfterSmds[] =
      "Network.Ash.Cellular.ESim.UserInstall.UserErrorsFiltered."
      "ViaQrCodeAfterSmds";
  static constexpr char
      kESimUserInstallUserErrorsFilteredViaQrCodeSkippedSmds[] =
          "Network.Ash.Cellular.ESim.UserInstall.UserErrorsFiltered."
          "ViaQrCodeSkippedSmds";
  static constexpr char kESimUserInstallUserErrorsFilteredViaSmds[] =
      "Network.Ash.Cellular.ESim.UserInstall.UserErrorsFiltered.ViaSmds";
  static constexpr char kESimUserInstallUserErrorsIncludedAll[] =
      "Network.Ash.Cellular.ESim.UserInstall.UserErrorsIncluded.All";
  static constexpr char
      kESimUserInstallUserErrorsIncludedViaActivationCodeAfterSmds[] =
          "Network.Ash.Cellular.ESim.UserInstall.UserErrorsIncluded."
          "ViaActivationCodeAfterSmds";
  static constexpr char
      kESimUserInstallUserErrorsIncludedViaActivationCodeSkippedSmds[] =
          "Network.Ash.Cellular.ESim.UserInstall.UserErrorsIncluded."
          "ViaActivationCodeSkippedSmds";
  static constexpr char kESimUserInstallUserErrorsIncludedViaQrCodeAfterSmds[] =
      "Network.Ash.Cellular.ESim.UserInstall.UserErrorsIncluded."
      "ViaQrCodeAfterSmds";
  static constexpr char
      kESimUserInstallUserErrorsIncludedViaQrCodeSkippedSmds[] =
          "Network.Ash.Cellular.ESim.UserInstall.UserErrorsIncluded."
          "ViaQrCodeSkippedSmds";
  static constexpr char kESimUserInstallUserErrorsIncludedViaSmds[] =
      "Network.Ash.Cellular.ESim.UserInstall.UserErrorsIncluded.ViaSmds";
  static constexpr char kESimSmdsScanOtherUserErrorsFiltered[] =
      "Network.Ash.Cellular.ESim.SmdsScan.Other.UserErrorsFiltered";
  static constexpr char kESimSmdsScanOtherUserErrorsIncluded[] =
      "Network.Ash.Cellular.ESim.SmdsScan.Other.UserErrorsIncluded";
  static constexpr char kESimSmdsScanAndroidUserErrorsFiltered[] =
      "Network.Ash.Cellular.ESim.SmdsScan.Android.UserErrorsFiltered";
  static constexpr char kESimSmdsScanAndroidUserErrorsIncluded[] =
      "Network.Ash.Cellular.ESim.SmdsScan.Android.UserErrorsIncluded";
  static constexpr char kESimSmdsScanGsmaUserErrorsFiltered[] =
      "Network.Ash.Cellular.ESim.SmdsScan.Gsma.UserErrorsFiltered";
  static constexpr char kESimSmdsScanGsmaUserErrorsIncluded[] =
      "Network.Ash.Cellular.ESim.SmdsScan.Gsma.UserErrorsIncluded";

  static constexpr char kUserAllowTextMessagesSuppressionStateHistogram[] =
      "Network.Ash.Cellular.AllowTextMessages.User.SuppressionState";
  static constexpr char kPolicyAllowTextMessagesSuppressionStateHistogram[] =
      "Network.Ash.Cellular.AllowTextMessages.Policy.SuppressionState";
  static constexpr char kAllowTextMessagesNotificationSuppressionState[] =
      "Network.Ash.Cellular.AllowTextMessages."
      "TextMessageNotificationSuppressionState";

  CellularNetworkMetricsLogger(
      NetworkStateHandler* network_state_handler,
      NetworkMetadataStore* network_metadata_store,
      ConnectionInfoMetricsLogger* connection_info_metrics_logger);
  CellularNetworkMetricsLogger(const CellularNetworkMetricsLogger&) = delete;
  CellularNetworkMetricsLogger& operator=(const CellularNetworkMetricsLogger&) =
      delete;
  ~CellularNetworkMetricsLogger() override;

  // Logs results from attempting operations related to custom APNs.
  static void LogCreateCustomApnResult(
      bool success,
      chromeos::network_config::mojom::ApnPropertiesPtr apn);
  static void LogCreateExclusivelyEnabledCustomApnResult(
      bool success,
      chromeos::network_config::mojom::ApnPropertiesPtr apn);
  static void LogRemoveCustomApnResult(
      bool success,
      std::vector<chromeos::network_config::mojom::ApnType> apn_types);
  static void LogModifyCustomApnResult(
      bool success,
      std::vector<chromeos::network_config::mojom::ApnType> old_apn_types,
      std::optional<chromeos::network_config::mojom::ApnState> apn_state,
      std::optional<chromeos::network_config::mojom::ApnState> old_apn_state);
  static void LogUnmanagedCustomApnMigrationType(
      UnmanagedApnMigrationType type);
  static void LogManagedCustomApnMigrationType(ManagedApnMigrationType type);

  // Logs results from attempting operations related to eSIM.
  static void LogSmdsScanProfileCount(size_t count, SmdsScanMethod method);
  static void LogSmdsScanDuration(const base::TimeDelta& duration,
                                  bool success,
                                  const std::string& smds_activation_code);

  static void LogESimUserInstallMethod(ESimUserInstallMethod method);
  static void LogESimPolicyInstallMethod(ESimPolicyInstallMethod method);
  static void LogESimPolicyInstallNoAvailableProfiles(
      ESimPolicyInstallMethod method);
  // The |is_user_error| parameter is used to indicate that |result| was caused
  // by something outside the control of ChromeOS, e.g. an invalid activation
  // code, and is not actionable. We do not include this category of errors in
  // the "filtered" version of our histograms so that we may have a better
  // understanding of eSIM installations on ChromeOS with minimal noise.
  static void LogESimUserInstallResult(ESimUserInstallMethod method,
                                       ESimOperationResult result,
                                       bool is_user_error);
  static void LogESimPolicyInstallResult(ESimPolicyInstallMethod method,
                                         ESimOperationResult result,
                                         bool is_initial,
                                         bool is_user_error);
  // Record the result of a single SM-DS scan of a single SM-DS server. When
  // |status| is not provided this function assumes that we failed to inhibit
  // the cellular device.
  static void LogSmdsScanResult(const std::string& smds_activation_code,
                                std::optional<HermesResponseStatus> status);

  // Returns the eSIM installation result for the provided Hermes response
  // status. When the status is unavailable, assume that we failed to inhibit
  // the cellular device.
  static ESimOperationResult ComputeESimOperationResult(
      std::optional<HermesResponseStatus> status);

  // Returns whether |status| is considered a "user error" and should be
  // filtered when emitting to eSIM installation result histograms. An Hermes
  // response status is considered a "user error" when we believe the result is
  // due to an error or situation outside the control of ChromeOS, e.g. an
  // invalid activation code.
  static bool HermesResponseStatusIsUserError(HermesResponseStatus status);

  static void LogUserTextMessageSuppressionState(
      ash::UserTextMessageSuppressionState state);
  static void LogPolicyTextMessageSuppressionState(
      ash::PolicyTextMessageSuppressionState state);

  static void LogTextMessageNotificationSuppressionState(
      NotificationSuppressionState state);

 private:
  // ConnectionInfoMetricsLogger::Observer:
  void OnConnectionResult(
      const std::string& guid,
      const std::optional<std::string>& shill_error) override;

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<NetworkMetadataStore, DanglingUntriaged> network_metadata_store_ =
      nullptr;

  base::ScopedObservation<ConnectionInfoMetricsLogger,
                          ConnectionInfoMetricsLogger::Observer>
      connection_info_metrics_logger_observation_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_
