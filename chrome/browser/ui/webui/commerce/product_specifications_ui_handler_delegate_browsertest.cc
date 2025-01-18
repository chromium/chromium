// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/product_specifications_ui_handler_delegate.h"

#include "base/json/json_reader.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_web_ui.h"

namespace commerce {
namespace {

const char kExampleUrl[] = "http://example.com/";

class NewBrowserObserver : public BrowserListObserver {
 public:
  NewBrowserObserver() { BrowserList::AddObserver(this); }
  NewBrowserObserver(const NewBrowserObserver&) = delete;
  NewBrowserObserver& operator=(const NewBrowserObserver&) = delete;
  ~NewBrowserObserver() override { BrowserList::RemoveObserver(this); }

  void Wait() {
    if (browsers_.size() == 0) {
      run_loop_.Run();
    }
  }

  std::vector<Browser*>& GetBrowsers() { return browsers_; }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    LOG(ERROR) << "browser added";
    browsers_.push_back(browser);
    run_loop_.Quit();
  }

 private:
  std::vector<Browser*> browsers_;
  base::RunLoop run_loop_;
};

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
      std::make_unique<ProductSpecificationsUIHandlerDelegate>(web_ui_.get());
  delegate->ShowDisclosureDialog(urls, name, id);

  auto* dialog =
      ProductSpecificationsDisclosureDialog::current_instance_for_testing();
  ASSERT_TRUE(dialog);

  // Check dialog args.
  auto dict = base::JSONReader::ReadDict(dialog->GetDialogArgs());
  ASSERT_TRUE(dict.has_value());
  auto* set_name = dict->FindString(kDialogArgsName);
  ASSERT_TRUE(set_name);
  ASSERT_EQ(name, *set_name);
  auto* url_list = dict->FindList(kDialogArgsUrls);
  ASSERT_TRUE(url_list);
  ASSERT_EQ(1u, url_list->size());
  ASSERT_EQ(urls[0].spec(), (*url_list)[0].GetString());
  auto* set_id = dict->FindString(kDialogArgsSetId);
  ASSERT_TRUE(set_id);
  ASSERT_EQ(id, *set_id);
  auto new_tab = dict->FindBool(kDialogArgsInNewTab);
  ASSERT_TRUE(new_tab.has_value());
  ASSERT_FALSE(new_tab.value());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsUIHandlerDelegateBrowserTest,
                       TestShowProductSpecificationsSetForUuid) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  auto delegate =
      std::make_unique<ProductSpecificationsUIHandlerDelegate>(web_ui_.get());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  content::TestNavigationObserver observer_one(
      GetProductSpecsTabUrlForID(uuid));
  observer_one.WatchExistingWebContents();

  delegate->ShowProductSpecificationsSetForUuid(uuid, false);

  observer_one.Wait();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(GetProductSpecsTabUrlForID(uuid), browser()
                                                  ->tab_strip_model()
                                                  ->GetActiveWebContents()
                                                  ->GetLastCommittedURL());

  content::TestNavigationObserver observer_two(
      GetProductSpecsTabUrlForID(uuid));
  observer_two.StartWatchingNewWebContents();

  delegate->ShowProductSpecificationsSetForUuid(uuid, true);

  observer_two.WaitForNavigationFinished();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(GetProductSpecsTabUrlForID(uuid), browser()
                                                  ->tab_strip_model()
                                                  ->GetActiveWebContents()
                                                  ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsUIHandlerDelegateBrowserTest,
                       TestShowComparePage_InCurrentTab) {
  auto delegate =
      std::make_unique<ProductSpecificationsUIHandlerDelegate>(web_ui_.get());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  const auto compare_url = GURL(kChromeUICompareUrl);
  content::TestNavigationObserver observer(compare_url);
  observer.WatchExistingWebContents();

  delegate->ShowComparePage(false);

  observer.Wait();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(compare_url, browser()
                             ->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsUIHandlerDelegateBrowserTest,
                       TestShowComparePage_InNewTab) {
  auto delegate =
      std::make_unique<ProductSpecificationsUIHandlerDelegate>(web_ui_.get());

  const auto compare_url = GURL(kChromeUICompareUrl);
  content::TestNavigationObserver observer(compare_url);
  observer.StartWatchingNewWebContents();

  delegate->ShowComparePage(true);

  observer.WaitForNavigationFinished();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(compare_url, browser()
                             ->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsUIHandlerDelegateBrowserTest,
                       TestShowProductSpecificationSetsForUuids_InNewTabs) {
  const base::Uuid uuid_one = base::Uuid::GenerateRandomV4();
  const base::Uuid uuid_two = base::Uuid::GenerateRandomV4();
  auto delegate =
      std::make_unique<ProductSpecificationsUIHandlerDelegate>(web_ui_.get());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  content::TestNavigationObserver observer_one(
      GetProductSpecsTabUrlForID(uuid_one));
  observer_one.StartWatchingNewWebContents();
  content::TestNavigationObserver observer_two(
      GetProductSpecsTabUrlForID(uuid_two));
  observer_two.StartWatchingNewWebContents();

  delegate->ShowProductSpecificationsSetsForUuids(
      {uuid_one, uuid_two},
      product_specifications::mojom::ShowSetDisposition::kInNewTabs);

  observer_one.Wait();
  observer_two.Wait();

  // First tab is the originally open tab.
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_EQ(
      GetProductSpecsTabUrlForID(uuid_one),
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetLastCommittedURL());
  ASSERT_EQ(
      GetProductSpecsTabUrlForID(uuid_two),
      browser()->tab_strip_model()->GetWebContentsAt(2)->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsUIHandlerDelegateBrowserTest,
                       TestShowProductSpecificationSetsForUuids_InNewWindow) {
  const base::Uuid uuid_one = base::Uuid::GenerateRandomV4();
  const base::Uuid uuid_two = base::Uuid::GenerateRandomV4();
  auto delegate =
      std::make_unique<ProductSpecificationsUIHandlerDelegate>(web_ui_.get());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  NewBrowserObserver browser_observer;

  content::TestNavigationObserver observer_one(
      GetProductSpecsTabUrlForID(uuid_one));
  observer_one.StartWatchingNewWebContents();
  content::TestNavigationObserver observer_two(
      GetProductSpecsTabUrlForID(uuid_two));
  observer_two.StartWatchingNewWebContents();

  delegate->ShowProductSpecificationsSetsForUuids(
      {uuid_one, uuid_two},
      product_specifications::mojom::ShowSetDisposition::kInNewWindow);

  observer_one.Wait();
  observer_two.Wait();
  browser_observer.Wait();

  // The original browser window should still only have one tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Only one new browser should have been opened with the new tabs.
  const auto& browsers = browser_observer.GetBrowsers();
  ASSERT_EQ(1UL, browsers.size());
  const auto* new_browser = browsers.at(0);
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(GetProductSpecsTabUrlForID(uuid_one), new_browser->tab_strip_model()
                                                      ->GetWebContentsAt(0)
                                                      ->GetLastCommittedURL());
  ASSERT_EQ(GetProductSpecsTabUrlForID(uuid_two), new_browser->tab_strip_model()
                                                      ->GetWebContentsAt(1)
                                                      ->GetLastCommittedURL());
}

}  // namespace commerce
