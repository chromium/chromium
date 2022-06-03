// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/cleaner/noop_cleaner_impl.h"

#include <utility>

#include "base/notreached.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

NoopCleanerImpl::NoopCleanerImpl() = default;
NoopCleanerImpl::~NoopCleanerImpl() = default;

void NoopCleanerImpl::Start(const std::vector<UwSId>& uws_ids,
                            DoneCallback done_callback) {
  // NoopCleanerImpl should only be used by engines that don't support
  // cleaning, so Start should never be called.
  NOTREACHED();
  std::move(done_callback).Run(RESULT_CODE_CLEANUP_NOT_SUPPORTED_BY_ENGINE);
}

void NoopCleanerImpl::StartPostReboot(const std::vector<UwSId>& uws_ids,
                                      DoneCallback done_callback) {
  // NoopCleanerImpl should only be used by engines that don't support
  // cleaning, so StartPostReboot should never be called.
  NOTREACHED();
  std::move(done_callback).Run(RESULT_CODE_CLEANUP_NOT_SUPPORTED_BY_ENGINE);
}

void NoopCleanerImpl::Stop() {
  // Do nothing.
}

bool NoopCleanerImpl::IsCompletelyDone() const {
  return true;
}

bool NoopCleanerImpl::CanClean(const std::vector<UwSId>& uws_ids) {
  // This should only be called if removable UwS is found, to verify that it's
  // safe to remove. But NoopCleanerImpl should only be used by engines that
  // have no removable UwS.
  NOTREACHED();
  return false;
}

}  // namespace chrome_cleaner
