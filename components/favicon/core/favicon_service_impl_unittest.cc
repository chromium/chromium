// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_service_impl.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/favicon/core/favicon_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace favicon {
namespace {

TEST(FaviconServiceImplTest, ShouldCacheUnableToDownloadFavicons) {
  base::ScopedTempDir history_dir;
  base::test::TaskEnvironment task_environment;
  CHECK(history_dir.CreateUniqueTempDir());
  std::unique_ptr<history::HistoryService> history_service =
      history::CreateHistoryService(history_dir.GetPath(), /*create_db=*/false);
  FaviconServiceImpl favicon_service(
      /*favicon_client=*/nullptr, history_service.get());

  const GURL icon1("http://www.google.com/favicon.ico");
  const GURL icon2("http://www.youtube.com/favicon.ico");
  EXPECT_FALSE(favicon_service.WasUnableToDownloadFavicon(icon1));
  EXPECT_FALSE(favicon_service.WasUnableToDownloadFavicon(icon2));

  favicon_service.UnableToDownloadFavicon(icon1);
  EXPECT_TRUE(favicon_service.WasUnableToDownloadFavicon(icon1));
  EXPECT_FALSE(favicon_service.WasUnableToDownloadFavicon(icon2));

  favicon_service.UnableToDownloadFavicon(icon2);
  EXPECT_TRUE(favicon_service.WasUnableToDownloadFavicon(icon2));

  favicon_service.ClearUnableToDownloadFavicons();
  EXPECT_FALSE(favicon_service.WasUnableToDownloadFavicon(icon1));
  EXPECT_FALSE(favicon_service.WasUnableToDownloadFavicon(icon2));
}

}  // namespace
}  // namespace favicon
