// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
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

// Performs a leak-check for {username, password} for Chrome signed-in users.
class AuthenticatedLeakCheck : public LeakDetectionCheck {
 public:
  AuthenticatedLeakCheck(
      LeakDetectionDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AuthenticatedLeakCheck() override;

  // Returns true if there is a Google account to use for the leak detection
  // check. Otherwise, instantiating the class is pointless.
  static bool HasAccountForRequest(
      const signin::IdentityManager* identity_manager);

  // LeakDetectionCheck:
  void Start(const GURL& url,
             base::string16 username,
             base::string16 password) override;

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
      std::string access_token,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Called when the single leak lookup request is done. |response| is null in
  // case of an invalid server response, or contains a valid
  // SingleLookupResponse instance otherwise.
  void OnLookupSingleLeakResponse(
      std::unique_ptr<SingleLookupResponse> response);

  // Called when the network response is analazyed on the background thread. The
  // method is called on the main thread.
  void OnAnalyzeSingleLeakResponse(AnalyzeResponseResult result);

  // Delegate for the instance. Should outlive |this|.
  LeakDetectionDelegateInterface* const delegate_;
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
  base::string16 username_;
  // |password| passed to Start().
  base::string16 password_;
  // Encryption key used during the request.
  std::string encryption_key_;
  // Weak pointers for different callbacks.
  base::WeakPtrFactory<AuthenticatedLeakCheck> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_
