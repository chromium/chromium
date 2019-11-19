// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/common/search/mock_embedded_search_client.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

class MockEmbeddedSearchClientFactory
    : public SearchIPCRouter::EmbeddedSearchClientFactory {
 public:
  MOCK_METHOD0(GetEmbeddedSearchClient,
               chrome::mojom::EmbeddedSearchClient*(void));
};

}  // namespace

class SearchTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  SearchTabHelperTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SearchTabHelper::CreateForWebContents(web_contents());
    auto* search_tab = SearchTabHelper::FromWebContents(web_contents());
    auto factory =
        std::make_unique<NiceMock<MockEmbeddedSearchClientFactory>>();
    ON_CALL(*factory, GetEmbeddedSearchClient())
        .WillByDefault(Return(&mock_embedded_search_client_));
    search_tab->ipc_router_for_testing()
        .set_embedded_search_client_factory_for_testing(std::move(factory));
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {{ProfileSyncServiceFactory::GetInstance(),
             base::BindRepeating(
                 [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                   return std::make_unique<syncer::TestSyncService>();
                 })}};
  }

  // Configure the account to |sync_history| or not.
  void SetHistorySync(bool sync_history) {
    syncer::TestSyncService* sync_service =
        static_cast<syncer::TestSyncService*>(
            ProfileSyncServiceFactory::GetForProfile(profile()));

    sync_service->SetFirstSetupComplete(true);
    syncer::ModelTypeSet types;
    if (sync_history) {
      types.Put(syncer::TYPED_URLS);
    }
    sync_service->SetPreferredDataTypes(types);
  }

 private:
  NiceMock<MockEmbeddedSearchClient> mock_embedded_search_client_;
};

TEST_F(SearchTabHelperTest, FileSelectedUpdatesLastSelectedDirectory) {
  NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  base::FilePath filePath =
      base::FilePath::FromUTF8Unsafe("a/b/c/Picture/kitten.png");
  search_tab_helper->FileSelected(filePath, 0, {});
  Profile* profile = search_tab_helper->profile();
  EXPECT_EQ(filePath.DirName(), profile->last_selected_directory());
}

TEST_F(SearchTabHelperTest, TitleIsSetForNTP) {
  NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE),
            web_contents()->GetTitle());
}
