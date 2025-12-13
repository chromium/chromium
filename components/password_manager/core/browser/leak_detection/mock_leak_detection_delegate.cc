// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"

#include "build/build_config.h"

namespace password_manager {

MockLeakDetectionDelegateInterface::MockLeakDetectionDelegateInterface() =
    default;
MockLeakDetectionDelegateInterface::~MockLeakDetectionDelegateInterface() =
    default;

#if !BUILDFLAG(IS_ANDROID)
MockBulkLeakCheckDelegateInterface::MockBulkLeakCheckDelegateInterface() =
    default;
MockBulkLeakCheckDelegateInterface::~MockBulkLeakCheckDelegateInterface() =
    default;
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
