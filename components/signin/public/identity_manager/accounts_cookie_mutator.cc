// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"

#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace signin {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
std::unique_ptr<BoundSessionOAuthMultiLoginDelegate> AccountsCookieMutator::
    PartitionDelegate::CreateBoundSessionOAuthMultiLoginDelegateForPartition() {
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
