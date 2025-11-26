// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"

namespace signin {

std::unique_ptr<BoundSessionOAuthMultiLoginDelegate> AccountsCookieMutator::
    PartitionDelegate::CreateBoundSessionOAuthMultiLoginDelegateForPartition() {
  return nullptr;
}

}  // namespace signin
