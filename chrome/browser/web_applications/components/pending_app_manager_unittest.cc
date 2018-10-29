// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/pending_app_manager.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class PendingAppManagerTest : public testing::Test {
 protected:
  void Sync(std::vector<GURL> urls) {
    pending_app_manager_.ResetCounts();

    std::vector<PendingAppManager::AppInfo> app_infos;
    for (const auto& url : urls) {
      app_infos.emplace_back(url, LaunchContainer::kWindow,
                             InstallSource::kInternal);
    }
    pending_app_manager_.SynchronizeInstalledApps(std::move(app_infos),
                                                  InstallSource::kInternal);
  }

  void Expect(int deduped_install_count,
              int deduped_uninstall_count,
              std::vector<GURL> installed_app_urls) {
    EXPECT_EQ(deduped_install_count,
              pending_app_manager_.deduped_install_count());
    EXPECT_EQ(deduped_uninstall_count,
              pending_app_manager_.deduped_uninstall_count());

    std::vector<GURL> urls =
        pending_app_manager_.GetInstalledAppUrls(InstallSource::kInternal);
    std::sort(urls.begin(), urls.end());
    EXPECT_EQ(installed_app_urls, urls);
  }

  TestPendingAppManager pending_app_manager_;
};

TEST_F(PendingAppManagerTest, SynchronizeInstalledApps) {
  GURL a("https://a.example.com/");
  GURL b("https://b.example.com/");
  GURL c("https://c.example.com/");
  GURL d("https://d.example.com/");
  GURL e("https://e.example.com/");

  Sync(std::vector<GURL>{a, b, d});
  Expect(3, 0, std::vector<GURL>{a, b, d});

  Sync(std::vector<GURL>{b, e});
  Expect(1, 2, std::vector<GURL>{b, e});

  Sync(std::vector<GURL>{e});
  Expect(0, 1, std::vector<GURL>{e});

  Sync(std::vector<GURL>{c});
  Expect(1, 1, std::vector<GURL>{c});

  Sync(std::vector<GURL>{e, a, d});
  Expect(3, 1, std::vector<GURL>{a, d, e});

  Sync(std::vector<GURL>{c, a, b, d, e});
  Expect(2, 0, std::vector<GURL>{a, b, c, d, e});

  Sync(std::vector<GURL>{});
  Expect(0, 5, std::vector<GURL>{});

  // The remaining code tests duplicate inputs.

  Sync(std::vector<GURL>{b, a, b, c});
  Expect(3, 0, std::vector<GURL>{a, b, c});

  Sync(std::vector<GURL>{e, a, e, e, e, a});
  Expect(1, 2, std::vector<GURL>{a, e});

  Sync(std::vector<GURL>{b, c, d});
  Expect(3, 2, std::vector<GURL>{b, c, d});

  Sync(std::vector<GURL>{a, a, a, a, a, a});
  Expect(1, 3, std::vector<GURL>{a});

  Sync(std::vector<GURL>{});
  Expect(0, 1, std::vector<GURL>{});
}

}  // namespace web_app
