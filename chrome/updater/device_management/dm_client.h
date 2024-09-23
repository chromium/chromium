// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DEVICE_MANAGEMENT_DM_CLIENT_H_
#define CHROME_UPDATER_DEVICE_MANAGEMENT_DM_CLIENT_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"

class GURL;

namespace update_client {
class NetworkFetcher;
}

namespace updater {

class DMStorage;
struct PolicyServiceProxyConfiguration;
struct PolicyValidationResult;

class DMClient {
 public:
  class Configurator {
   public:
    virtual ~Configurator() = default;

    // URL at which to contact the DM server.
    virtual GURL GetDMServerUrl() const = 0;

    // Agent reported in the "agent" query parameter.
    virtual std::string GetAgentParameter() const = 0;

    // The platform reported in the "platform" query parameter.
    virtual std::string GetPlatformParameter() const = 0;

    virtual std::unique_ptr<update_client::NetworkFetcher>
    CreateNetworkFetcher() const = 0;
  };

  enum class RequestResult {
    // DM request is completed successfully.
    kSuccess = 0,

    // Request is not sent because there's no device ID.
    kNoDeviceID,

    // Register request is not sent since the device is already registered.
    kAlreadyRegistered,

    // Request is not sent because the device is not managed.
    kNotManaged,

    // Request is not sent because the device is de-registered.
    kDeregistered,

    // Policy fetch request is not sent because there's no DM token.
    kNoDMToken,

    // Request is not sent because network fetcher fails to create.
    kFetcherError,

    // Request failed with network error.
    kNetworkError,

    // Request failed with an HTTP error from server.
    kHttpError,

    // Failed to persist the response into storage.
    kSerializationError,

    // Got an unexpected response for the request.
    kUnexpectedResponse,

    // No POST data.
    kNoPayload,

    // Failed to get the default DM storage.
    kNoDefaultDMStorage,
  };

  using RegisterCallback = base::OnceCallback<void(RequestResult)>;

  using PolicyFetchCallback = base::OnceCallback<void(
      RequestResult,
      const std::vector<PolicyValidationResult>& validation_results)>;

  using PolicyValidationReportCallback =
      base::OnceCallback<void(RequestResult)>;

  // Sends a device registration request to DM server.
  // Device must complete registration before actual management.
  // Possible outcome:
  //   1) Registration is skipped if one of the following is true:
  //      a) There's no enrollment token.
  //      b) Device is already registered.
  //      c) Device is explicitly unregistered by server.
  //   2) Registration completes successfully and a DM token is saved in
  //      storage.
  //   3) Server unregisters the device and the device is marked as such.
  //   4) Registration fails, device status is not changed.
  //
  static void RegisterDevice(
      std::unique_ptr<Configurator> config,
      scoped_refptr<device_management_storage::DMStorage> storage,
      RegisterCallback callback);

  // Fetches policies from the DM server.
  // Possible outcome:
  //   1) Policy fetch is skipped when there is no valid DM token.
  //   2) Policy fetch completes successfully. New policies will be validated
  //      and saved into storage. Cached info will be updated if the policy
  //      contains a new public key.
  //   3) Server unregisters the device, all policies will be cleaned and device
  //      exits management.
  //   4) Fetch fails, device status is not changed.
  //
  static void FetchPolicy(
      std::unique_ptr<Configurator> config,
      scoped_refptr<device_management_storage::DMStorage> storage,
      PolicyFetchCallback callback);

  // Posts the policy validation report back to DM server.
  // The report request is skipped if there's no valid DM token or
  // `validation_result` has no error to report.
  // The report is best-effort only. No retry will be attempted if it fails.
  //
  static void ReportPolicyValidationErrors(
      std::unique_ptr<Configurator> config,
      scoped_refptr<device_management_storage::DMStorage> storage,
      const PolicyValidationResult& validation_result,
      PolicyValidationReportCallback callback);

  static std::unique_ptr<Configurator> CreateDefaultConfigurator(
      const GURL& server_url,
      std::optional<PolicyServiceProxyConfiguration>
          policy_service_proxy_configuration);
};

std::ostream& operator<<(std::ostream& os,
                         const DMClient::RequestResult& result);

}  // namespace updater

#endif  // CHROME_UPDATER_DEVICE_MANAGEMENT_DM_CLIENT_H_
