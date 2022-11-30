// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/default_search_icon_source.h"

#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"

class DefaultSearchIconBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Wait for the template url service to load.
    search_test_utils::WaitForTemplateURLServiceToLoad(GetTemplateURLService());
  }

  // Sets a new default search provider from the default search engine list.
  void SetNewDefaultSearch() {
    auto* template_url_service = GetTemplateURLService();
    auto* current_default_template_url =
        template_url_service->GetDefaultSearchProvider();

    TemplateURLService::TemplateURLVector template_urls =
        template_url_service->GetTemplateURLs();
    auto iter = base::ranges::find_if_not(
        template_urls, [&](const TemplateURL* template_url) {
          return current_default_template_url == template_url;
        });

    ASSERT_NE(template_urls.end(), iter);
    template_url_service->SetUserSelectedDefaultSearchProvider(*iter);
  }

  TemplateURLService* GetTemplateURLService() {
    return TemplateURLServiceFactory::GetForProfile(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(DefaultSearchIconBrowserTest,
                       ClientNotifiedOfTemplateURLServiceChange) {
  // Tracks whether the client was notified of a change to the default search
  // icon source.
  bool client_notified = false;
  const auto subscription =
      DefaultSearchIconSource::GetOrCreateForBrowser(browser())
          ->RegisterIconChangedSubscription(
              base::BindLambdaForTesting([&]() { client_notified = true; }));

  // Clients should not have yet been notified of an icon change.
  EXPECT_FALSE(client_notified);

  // The client should be notified if the default search provider has changed.
  for (int i = 0; i < 3; ++i) {
    SetNewDefaultSearch();
    EXPECT_TRUE(client_notified);
    client_notified = false;
  }
}
