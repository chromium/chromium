// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/test/test_helper.h"

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"

namespace media_router {

#if !defined(OS_ANDROID)
TestMediaSinkService::TestMediaSinkService()
    : TestMediaSinkService(base::DoNothing()) {}

TestMediaSinkService::TestMediaSinkService(
    const OnSinksDiscoveredCallback& callback)
    : MediaSinkServiceBase(callback), timer_(new base::MockOneShotTimer()) {
  SetTimerForTest(base::WrapUnique(timer_));
}

TestMediaSinkService::~TestMediaSinkService() = default;
#endif  // !defined(OS_ANDROID)

}  // namespace media_router
