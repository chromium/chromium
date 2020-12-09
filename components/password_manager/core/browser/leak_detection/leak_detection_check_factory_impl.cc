// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"

#include <utility>

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_impl.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

LeakDetectionCheckFactoryImpl::LeakDetectionCheckFactoryImpl() = default;
LeakDetectionCheckFactoryImpl::~LeakDetectionCheckFactoryImpl() = default;

std::unique_ptr<LeakDetectionCheck>
LeakDetectionCheckFactoryImpl::TryCreateLeakCheck(
    LeakDetectionDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) const {
  if (!AuthenticatedLeakCheck::HasAccountForRequest(identity_manager)) {
    delegate->OnError(LeakDetectionError::kNotSignIn);
    return nullptr;
  }

  return std::make_unique<AuthenticatedLeakCheck>(
      delegate, identity_manager, std::move(url_loader_factory));
}

std::unique_ptr<BulkLeakCheck>
LeakDetectionCheckFactoryImpl::TryCreateBulkLeakCheck(
    BulkLeakCheckDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) const {
  if (!AuthenticatedLeakCheck::HasAccountForRequest(identity_manager)) {
    delegate->OnError(LeakDetectionError::kNotSignIn);
    return nullptr;
  }
  return std::make_unique<BulkLeakCheckImpl>(delegate, identity_manager,
                                             std::move(url_loader_factory));
}

}  // namespace password_manager
