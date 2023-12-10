// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace password_manager {

struct LookupSingleLeakPayload;
struct SingleLookupResponse;
enum class LeakDetectionError;

// Interface for the class making the network requests for leak detection.
class LeakDetectionRequestInterface {
 public:
  using LookupSingleLeakCallback =
      base::OnceCallback<void(std::unique_ptr<SingleLookupResponse>,
                              std::optional<LeakDetectionError>)>;

  LeakDetectionRequestInterface() = default;
  virtual ~LeakDetectionRequestInterface() = default;

  // Neither copyable nor movable.
  LeakDetectionRequestInterface(const LeakDetectionRequestInterface&) = delete;
  LeakDetectionRequestInterface& operator=(
      const LeakDetectionRequestInterface&) = delete;
  LeakDetectionRequestInterface(LeakDetectionRequestInterface&&) = delete;
  LeakDetectionRequestInterface& operator=(LeakDetectionRequestInterface&&) =
      delete;

  // Initiates a leak lookup network request for the credential corresponding to
  // |username_hash_prefix| and |encrypted_payload|.
  // |access_token| is required to authenticate the request for signed-in users.
  // |api_key| is required to authenticate the request for signed-out users.
  // Invokes |callback| on completion, unless this instance is deleted
  // beforehand. If the request failed, |callback| is invoked with |nullptr|,
  // otherwise a SingleLookupResponse is returned.
  virtual void LookupSingleLeak(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const std::optional<std::string>& access_token,
      const std::optional<std::string>& api_key,
      LookupSingleLeakPayload payload,
      LookupSingleLeakCallback callback) = 0;
};

// The factory for creating instances of  network requests for leak detection.
class LeakDetectionRequestFactory {
 public:
  LeakDetectionRequestFactory() = default;
  virtual ~LeakDetectionRequestFactory() = default;

  // Not copyable or movable
  LeakDetectionRequestFactory(const LeakDetectionRequestFactory&) = delete;
  LeakDetectionRequestFactory& operator=(const LeakDetectionRequestFactory&) =
      delete;
  LeakDetectionRequestFactory(LeakDetectionRequestFactory&&) = delete;
  LeakDetectionRequestFactory& operator=(LeakDetectionRequestFactory&&) =
      delete;

  virtual std::unique_ptr<LeakDetectionRequestInterface> CreateNetworkRequest()
      const;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_H_
