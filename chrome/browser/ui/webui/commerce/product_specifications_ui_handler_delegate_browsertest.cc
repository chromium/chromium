// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/product_specifications_ui_handler_delegate.h"

#include "base/json/json_reader.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_web_ui.h"

namespace {
const char kExampleUrl[] = "http://example.com/";
}  // namespace

class ProductSpecificationsUIHandlerDelegateBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  std::unique_ptr<content::TestWebUI> web_ui_;
};

IN_PROC_BROWSER_TEST_F(ProductSpecificationsUIHandlerDelegateBrowserTest,
                       TestShowDisclosureDialog) {
  std::vector<GURL> urls = {GURL(kExampleUrl)};
  std::string name = "test_name";
  std::string id = "test_id";
  auto delegate =
      std::make_unique<commerce::ProductSpecificationsUIHandlerDelegate>(
          web_ui_.get());
  delegate->ShowDisclosureDialog(urls, name, id);

  auto* dialog = commerce::ProductSpecificationsDisclosureDialog::
      current_instance_for_testing();
  ASSERT_TRUE(dialog);

  // Check dialog args.
  auto dict = base::JSONReader::ReadDict(dialog->GetDialogArgs());
  ASSERT_TRUE(dict.has_value());
  auto* set_name = dict->FindString(commerce::kDialogArgsName);
  ASSERT_TRUE(set_name);
  ASSERT_EQ(name, *set_name);
  auto* url_list = dict->FindList(commerce::kDialogArgsUrls);
  ASSERT_TRUE(url_list);
  ASSERT_EQ(1u, url_list->size());
  ASSERT_EQ(urls[0].spec(), (*url_list)[0].GetString());
  auto* set_id = dict->FindString(commerce::kDialogArgsSetId);
  ASSERT_TRUE(set_id);
  ASSERT_EQ(id, *set_id);
  auto new_tab = dict->FindBool(commerce::kDialogArgsInNewTab);
  ASSERT_TRUE(new_tab.has_value());
  ASSERT_FALSE(new_tab.value());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsUIHandlerDelegateBrowserTest,
                       TestShowProductSpecificationsSetForUuid) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  auto delegate =
      std::make_unique<commerce::ProductSpecificationsUIHandlerDelegate>(
          web_ui_.get());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  content::TestNavigationObserver observer_one(
      commerce::GetProductSpecsTabUrlForID(uuid));
  observer_one.WatchExistingWebContents();

  delegate->ShowProductSpecificationsSetForUuid(uuid, false);

  observer_one.Wait();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(commerce::GetProductSpecsTabUrlForID(uuid),
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());

  content::TestNavigationObserver observer_two(
      commerce::GetProductSpecsTabUrlForID(uuid));
  observer_two.StartWatchingNewWebContents();

  delegate->ShowProductSpecificationsSetForUuid(uuid, true);

  observer_two.WaitForNavigationFinished();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(commerce::GetProductSpecsTabUrlForID(uuid),
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}
