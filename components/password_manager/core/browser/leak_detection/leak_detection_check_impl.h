// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"
#include "url/gurl.h"

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace password_manager {

enum class AnalyzeResponseResult;
class LeakDetectionDelegateInterface;
struct LookupSingleLeakData;
struct SingleLookupResponse;

// Performs a leak-check for {username, password} for Chrome users.
class LeakDetectionCheckImpl : public LeakDetectionCheck {
 public:
  LeakDetectionCheckImpl(
      LeakDetectionDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::optional<std::string> api_key);
  ~LeakDetectionCheckImpl() override;

  // Returns true if there is a Google account to use for the leak detection
  // check.
  static bool HasAccountForRequest(
      const signin::IdentityManager* identity_manager);

  // LeakDetectionCheck:
  void Start(LeakDetectionInitiator initiator,
             const GURL& url,
             std::u16string username,
             std::u16string password) override;

#if defined(UNIT_TEST)
  void set_network_factory(
      std::unique_ptr<LeakDetectionRequestFactory> factory) {
    network_request_factory_ = std::move(factory);
  }
#endif  // defined(UNIT_TEST)

 private:
  class RequestPayloadHelper;

  // Called when the token request is done.
  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     signin::AccessTokenInfo access_token_info);

  // Called when the payload for the request is precomputed.
  void OnRequestDataReady(LookupSingleLeakData data);

  // Does the network request to check the credentials.
  void DoLeakRequest(
      LookupSingleLeakData data,
      std::optional<std::string> access_token,
      std::optional<std::string> api_key,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Called when the single leak lookup request is done. |response| is null in
  // case of an invalid server response, or contains a valid
  // SingleLookupResponse instance otherwise. |error| is set iff |response| is
  // null.
  void OnLookupSingleLeakResponse(
      std::unique_ptr<SingleLookupResponse> response,
      std::optional<LeakDetectionError> error);

  // Called when the network response is analazyed on the background thread. The
  // method is called on the main thread.
  void OnAnalyzeSingleLeakResponse(AnalyzeResponseResult result);

  // Delegate for the instance. Should outlive |this|.
  const raw_ptr<LeakDetectionDelegateInterface> delegate_;
  // Helper class to asynchronously prepare the data for the request.
  std::unique_ptr<RequestPayloadHelper> payload_helper_;
  // Class used to initiate a request to the identity leak lookup endpoint. This
  // is only instantiated if a valid |access_token_| could be obtained.
  std::unique_ptr<LeakDetectionRequestInterface> request_;
  // A factory for creating a |request_|.
  std::unique_ptr<LeakDetectionRequestFactory> network_request_factory_;
  // |url| passed to Start().
  GURL url_;
  // |username| passed to Start().
  std::u16string username_;
  // |password| passed to Start().
  std::u16string password_;
  // Encryption key used during the request.
  std::string encryption_key_;
  // Weak pointers for different callbacks.
  base::WeakPtrFactory<LeakDetectionCheckImpl> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_IMPL_H_
