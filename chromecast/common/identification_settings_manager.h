// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_IDENTIFICATION_SETTINGS_MANAGER_H_
#define CHROMECAST_COMMON_IDENTIFICATION_SETTINGS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromecast/common/cast_url_loader_throttle.h"
#include "chromecast/common/mojom/identification_settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace chromecast {

// Receives messages from the browser process and stores identification settings
// to feed into URLLoaderThrottles for throttling url requests in the browser or
// renderers. When constructed in a renderer, this class could be deleted on a
// different thread from the main thread.
class IdentificationSettingsManager
    : public mojom::IdentificationSettingsManager,
      public CastURLLoaderThrottle::Delegate {
 public:
  using RequestCompletionCallback = base::OnceCallback<
      void(int, net::HttpRequestHeaders, net::HttpRequestHeaders)>;
  using DoneSigningCallback = base::OnceCallback<void()>;
  using EnsureCertsCallback = base::OnceCallback<void()>;

  IdentificationSettingsManager();
  IdentificationSettingsManager(const IdentificationSettingsManager&) = delete;
  IdentificationSettingsManager& operator=(
      const IdentificationSettingsManager&) = delete;

  // CastURLLoaderThrottle::Delegate implementation:
  // |callback| will only be run if net::IO_PENDING is returned.
  int WillStartResourceRequest(network::ResourceRequest* request,
                               const std::string& /* session_id */,
                               RequestCompletionCallback callback) override;

  // chromecast::mojom::IdentificationSettingsManager implementation:
  void SetSubstitutableParameters(
      std::vector<mojom::SubstitutableParameterPtr> params) override;
  void SetClientAuth(mojo::PendingRemote<mojom::ClientAuthDelegate>
                         client_auth_delegate) override;
  void UpdateAppSettings(mojom::AppSettingsPtr app_settings) override;
  void UpdateDeviceSettings(mojom::DeviceSettingsPtr device_settings) override;
  void UpdateSubstitutableParamValues(
      std::vector<mojom::IndexValuePairPtr> updated_values) override;
  void UpdateBackgroundMode(bool background_mode) override;

 protected:
  ~IdentificationSettingsManager() override;

 private:
  struct RequestInfo;

  struct SubstitutableParameter {
    SubstitutableParameter();
    ~SubstitutableParameter();

    // Explicitly allows copy and move.
    SubstitutableParameter(const SubstitutableParameter& other);
    SubstitutableParameter& operator=(const SubstitutableParameter& other) =
        default;
    SubstitutableParameter(SubstitutableParameter&& other) noexcept;
    SubstitutableParameter& operator=(SubstitutableParameter&& other) noexcept =
        default;

    uint32_t index;
    std::string name;
    std::string replacement_token;
    std::string suppression_token;
    bool is_signature = false;
    bool suppress_header = false;
    bool need_query = false;
    std::string value;
  };

  SubstitutableParameter ConvertSubstitutableParameterFromMojom(
      mojom::SubstitutableParameterPtr mojo_param);

  void MoveCorsExemptHeaders(net::HttpRequestHeaders* headers,
                             net::HttpRequestHeaders* cors_exempt_headers);
  void HandlePendingRequests();
  GURL FindReplacementURL(const GURL& gurl) const;

  bool IsAllowed(const GURL& gurl);
  bool IsAppAllowedForDeviceIdentification() const;
  int ApplyBackgroundQueryParamIfNeeded(const GURL& orig_url,
                                        GURL& new_url) const;
  int ApplyDeviceIdentificationSettings(const GURL& orig_url,
                                        GURL& new_url,
                                        net::HttpRequestHeaders* headers);
  int ApplyHeaderChanges(const std::vector<SubstitutableParameter>& params,
                         net::HttpRequestHeaders* headers);
  int ApplyURLReplacementSettings(const GURL& request_url,
                                  const GURL& replacement_url,
                                  GURL& new_url) const;
  int EnsureSignature();
  int CreateSignatureAsync();
  void SignatureComplete(
      DoneSigningCallback done_signing,
      std::vector<chromecast::mojom::IndexValuePairPtr> signature_headers,
      base::Time next_refresh_time);
  void InitCerts(
      std::vector<chromecast::mojom::IndexValuePairPtr> cert_headers);

  // The following methods must only be accessed while holding exclusive
  // ownership of |lock_| for thread-safe access. Asserted at compile time via
  // EXCLUSIVE_LOCKS_REQUIRED and run-time via base::Lock::AssertAcquired.
  int EnsureCerts() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void ReplaceURL(const GURL& source,
                  const GURL& replacement,
                  GURL& new_url) const;

  bool NeedParameter(const SubstitutableParameter& param, uint32_t index) const;
  bool NeedsSignature(
      const std::vector<SubstitutableParameter>& parameters) const;
  void AnalyzeAndReplaceQueryString(
      const GURL& orig_url,
      GURL& new_url,
      std::vector<SubstitutableParameter>* params);
  void AddHttpHeaders(const std::vector<SubstitutableParameter>& parameters,
                      net::HttpRequestHeaders* headers) const;

  // A list of parameters used to replace some patterns in the template strings
  // i.e. JWT template. It is also used to fill in http headers.
  std::vector<SubstitutableParameter> substitutable_params_;

  // A set of headers which rarely change.
  base::flat_map<std::string /* header_name */, std::string /* header_value */>
      static_headers_;

  // Whether the app is in background mode.
  bool background_mode_ = false;

  // Whether the app is allowed for device identification.
  bool is_allowed_for_device_identification_ = false;

  // Bit representation of allowed headers.
  int allowed_headers_ = 0;

  // Host names used to match against http requests. The identification settings
  // are only applied if one of the host names matches the host name of the
  // request.
  std::vector<std::string> full_host_names_;
  std::vector<std::string> wildcard_host_names_;

  // Replacement map from canonical URL strings to GURLs.
  base::flat_map<std::string, GURL> replacements_;

  // Whether a request to generate certificates is sent.
  bool create_cert_in_progress_ GUARDED_BY(lock_) = false;

  // Whether a signature creation request is sent.
  bool create_signature_in_progress_ GUARDED_BY(lock_) = false;

  // Whether certificates have been initialized.
  bool cert_initialized_ GUARDED_BY(lock_) = false;

  // When the next time the signature needs to be refreshed.
  base::Time next_refresh_time_ GUARDED_BY(lock_);

  base::Clock* const clock_ GUARDED_BY(lock_);

  // TODO(b/159910473): Remove the lock if possible.
  mutable base::Lock lock_;

  mojo::Remote<mojom::ClientAuthDelegate> client_auth_delegate_;
  std::vector<std::unique_ptr<RequestInfo>> pending_requests_;
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_IDENTIFICATION_SETTINGS_MANAGER_H_
