// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_H_

#include <memory>
#include <string>

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace password_manager {

// This class encapsulates the logic required to talk to the identity leak check
// endpoint. Callers are expected to construct an instance for each request they
// would like to perform. Destruction of the class results in a cancellation of
// the initiated network request.
class LeakDetectionRequest : public LeakDetectionRequestInterface {
 public:
  // Enum representing different leak lookup response results. Needs to stay in
  // sync with the PasswordLeakLookupResponseResult enum in enums.xml.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LeakLookupResponseResult {
    kSuccess = 0,
    kFetchError = 1,
    kParseError = 2,
    kMaxValue = kParseError
  };

  static constexpr char kLookupSingleLeakEndpoint[] =
      "https://passwordsleakcheck-pa.googleapis.com/v1/leaks:lookupSingle";

  LeakDetectionRequest();
  ~LeakDetectionRequest() override;

  // Initiates a leak lookup network request for the credential corresponding to
  // |username_hash_prefix| and |encrypted_payload|.
  // |access_token| is required to authenticate the request for signed-in users.
  // |api_key| is required to authenticate the request for signed-out users.
  // Invokes |callback| on completion, unless this instance is deleted
  // beforehand. If the request failed, |callback| is invoked with |nullptr|,
  // otherwise a SingleLookupResponse is returned.
  void LookupSingleLeak(network::mojom::URLLoaderFactory* url_loader_factory,
                        const std::optional<std::string>& access_token,
                        const std::optional<std::string>& api_key,
                        LookupSingleLeakPayload payload,
                        LookupSingleLeakCallback callback) override;

 private:
  void OnLookupSingleLeakResponse(LookupSingleLeakCallback callback,
                                  std::unique_ptr<std::string> response);

  // Simple URL loader required for the network request to the identity
  // endpoint.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_H_
