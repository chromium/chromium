// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PERMISSION_REQUEST_CREATOR_IMPL_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PERMISSION_REQUEST_CREATOR_IMPL_H_

#include "components/supervised_user/core/browser/permission_request_creator.h"

#include <memory>

#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace supervised_user {

class PermissionRequestCreatorImpl : public PermissionRequestCreator {
 public:
  PermissionRequestCreatorImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PermissionRequestCreatorImpl() override;

  // PermissionRequestCreator implementation:
  bool IsEnabled() const override;
  void CreateURLAccessRequest(const GURL& url_requested,
                              SuccessCallback callback) override;

 private:
  // Dependencies.
  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PERMISSION_REQUEST_CREATOR_IMPL_H_
