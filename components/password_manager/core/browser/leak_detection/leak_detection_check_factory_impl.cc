// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"

#include <utility>

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
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
  // Instantiate the field trial right before the feature can be used. Thus,
  // the experiment groups will only contain the users who can use the feature.
  if (!base::FeatureList::IsEnabled(features::kLeakDetection))
    return nullptr;
  return std::make_unique<AuthenticatedLeakCheck>(
      delegate, identity_manager, std::move(url_loader_factory));
}

}  // namespace password_manager
