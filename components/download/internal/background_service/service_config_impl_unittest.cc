// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/service_config_impl.h"
#include "components/download/internal/background_service/config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

TEST(ServiceConfigImplTest, TestApi) {
  Configuration config;
  ServiceConfigImpl impl(&config);

  config.max_scheduled_downloads = 7;
  config.max_concurrent_downloads = 12;
  config.file_keep_alive_time = base::Seconds(12);

  EXPECT_EQ(config.max_scheduled_downloads,
            impl.GetMaxScheduledDownloadsPerClient());
  EXPECT_EQ(config.max_concurrent_downloads, impl.GetMaxConcurrentDownloads());
  EXPECT_EQ(config.file_keep_alive_time, impl.GetFileKeepAliveTime());
}

}  // namespace download
