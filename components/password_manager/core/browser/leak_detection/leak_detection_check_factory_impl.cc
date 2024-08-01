// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"

#include <utility>

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_impl.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_impl.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {
namespace {

// Returns |std::nullopt| for |signed_in_user|, as in this case authentication
// happens via access token. Otherwise returns API key for an appropriate
// |channel|.
std::optional<std::string> GetAPIKey(bool signed_in_user,
                                     version_info::Channel channel) {
  if (signed_in_user) {
    return std::nullopt;
  }
  return google_apis::GetAPIKey(channel);
}

}  // namespace

LeakDetectionCheckFactoryImpl::LeakDetectionCheckFactoryImpl() = default;
LeakDetectionCheckFactoryImpl::~LeakDetectionCheckFactoryImpl() = default;

std::unique_ptr<LeakDetectionCheck>
LeakDetectionCheckFactoryImpl::TryCreateLeakCheck(
    LeakDetectionDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel) const {
  CHECK(identity_manager);

  return std::make_unique<LeakDetectionCheckImpl>(
      delegate, identity_manager, std::move(url_loader_factory),
      GetAPIKey(LeakDetectionCheckImpl::HasAccountForRequest(identity_manager),
                channel));
}

std::unique_ptr<BulkLeakCheck>
LeakDetectionCheckFactoryImpl::TryCreateBulkLeakCheck(
    BulkLeakCheckDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) const {
  if (!LeakDetectionCheckImpl::HasAccountForRequest(identity_manager)) {
    delegate->OnError(LeakDetectionError::kNotSignIn);
    return nullptr;
  }
  return std::make_unique<BulkLeakCheckImpl>(delegate, identity_manager,
                                             std::move(url_loader_factory));
}

}  // namespace password_manager
