// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_TEST_TEST_HELPER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_TEST_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"

namespace media_router {

class MediaSink;

MediaSink CreateCastSink(const std::string& id, const std::string& name);
MediaSink CreateDialSink(const std::string& id, const std::string& name);
MediaSink CreateWiredDisplaySink(const std::string& id,
                                 const std::string& name);

#if !BUILDFLAG(IS_ANDROID)
class TestMediaSinkService : public MediaSinkServiceBase {
 public:
  TestMediaSinkService();
  explicit TestMediaSinkService(const OnSinksDiscoveredCallback& callback);

  TestMediaSinkService(const TestMediaSinkService&) = delete;
  TestMediaSinkService& operator=(const TestMediaSinkService&) = delete;

  ~TestMediaSinkService() override;

  base::MockOneShotTimer* timer() { return timer_; }

 private:
  // Owned by MediaSinkService.
  raw_ptr<base::MockOneShotTimer> timer_;
};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_TEST_TEST_HELPER_H_
