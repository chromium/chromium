// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_TEST_TEST_HELPER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_TEST_TEST_HELPER_H_

#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"

namespace media_router {

#if !defined(OS_ANDROID)
class TestMediaSinkService : public MediaSinkServiceBase {
 public:
  TestMediaSinkService();
  explicit TestMediaSinkService(const OnSinksDiscoveredCallback& callback);
  ~TestMediaSinkService() override;

  base::MockOneShotTimer* timer() { return timer_; }

 private:
  // Owned by MediaSinkService.
  base::MockOneShotTimer* timer_;
  DISALLOW_COPY_AND_ASSIGN(TestMediaSinkService);
};
#endif  // !defined(OS_ANDROID)

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_TEST_TEST_HELPER_H_
