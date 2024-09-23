// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_INFO_FETCHER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_INFO_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}

namespace policy {

enum class EnterpriseUserInfoFetchStatus {
  kSuccess,
  kFailedWithNetworkError,
  kCantParseJsonInResponse,
  kResponseIsNotDict,
  kMaxValue = kResponseIsNotDict
};

// Class that makes a UserInfo request, parses the response, and notifies
// a provided Delegate when the request is complete.
class POLICY_EXPORT UserInfoFetcher {
 public:
  class POLICY_EXPORT Delegate {
   public:
    // Invoked when the UserInfo request has succeeded, passing the parsed
    // response in |response|. Delegate may free the UserInfoFetcher in this
    // callback.
    virtual void OnGetUserInfoSuccess(const base::Value::Dict& response) = 0;

    // Invoked when the UserInfo request has failed, passing the associated
    // error in |error|. Delegate may free the UserInfoFetcher in this
    // callback.
    virtual void OnGetUserInfoFailure(const GoogleServiceAuthError& error) = 0;
  };

  // Create a new UserInfoFetcher. |url_loader_factory| can be nullptr for unit
  // tests.
  UserInfoFetcher(
      Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  UserInfoFetcher(const UserInfoFetcher&) = delete;
  UserInfoFetcher& operator=(const UserInfoFetcher&) = delete;
  ~UserInfoFetcher();

  // Starts the UserInfo request, using the passed OAuth2 |access_token|.
  void Start(const std::string& access_token);

  // Called by |url_loader_| on completion.
  void OnFetchComplete(std::unique_ptr<std::string> body);

 private:
  raw_ptr<Delegate> delegate_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_INFO_FETCHER_H_
