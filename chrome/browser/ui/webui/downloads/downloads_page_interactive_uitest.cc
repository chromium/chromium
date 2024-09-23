// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDownloadsPageTabId);

const char kClickFn[] = "el => el.click()";

using ::testing::Eq;

class DownloadsPageInteractiveUitest
    : public InteractiveBrowserTestT<DownloadTestBase> {
 public:
  DownloadsPageInteractiveUitest() {
    feature_list_.InitAndEnableFeature(
        safe_browsing::kDangerousDownloadInterstitial);
  }
  ~DownloadsPageInteractiveUitest() override = default;

  void SetUp() override {
    // This is necessary so that opening the downloads page via app menu will
    // reliably open in the same tab. (It only opens in the same tab if the
    // current tab is about:blank.)
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTestT<DownloadTestBase>::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTestT<DownloadTestBase>::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Opens chrome://downloads via the app menu. All tests must begin with this
  // step so that the identifier kDownloadsPageTabId is bound properly.
  auto OpenDownloadsPage() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadyEvent);
    const DeepQuery kPathToDocumentHtml{
        "html",
    };
    const char kNotLoadingFn[] = "el => el.getAttribute('loading') === null";
    // Waits for the page to finish loading. (This attribute is applied manually
    // by the WebUI and has a TODO to be converted to a class instead, so this
    // may be fragile.)
    StateChange html_not_loading;
    html_not_loading.type = StateChange::Type::kExistsAndConditionTrue;
    html_not_loading.where = kPathToDocumentHtml;
    html_not_loading.test_function = kNotLoadingFn;
    html_not_loading.event = kReadyEvent;
    return Steps(InstrumentTab(kDownloadsPageTabId),
                 PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
                 WaitForWebContentsNavigation(
                     kDownloadsPageTabId, GURL(chrome::kChromeUIDownloadsURL)),
                 WaitForStateChange(kDownloadsPageTabId, html_not_loading));
  }

  // Path to an element in the topmost download item.
  DeepQuery PathToTopmostItemElement(const std::string& element_selector) {
    return DeepQuery{
        "downloads-manager",
        "downloads-item",
        element_selector,
    };
  }

  // Checks that an element is visible (or not visible) on the page.
  auto CheckElementVisible(const DeepQuery& where, bool visible) {
    return CheckJsResultAt(kDownloadsPageTabId, where,
                           R"(el => {
                               rect = el.getBoundingClientRect();
                               return rect.height * rect.width > 0;
                            })",
                           Eq(visible));
  }

  // Finds the first download item on the page and presses a button in its
  // dropdown actions menu (which is required to be visible).
  auto TakeTopmostItemMenuAction(const std::string& menu_item_selector) {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadyEvent);
    const DeepQuery path_to_menu_button =
        PathToTopmostItemElement("#more-actions");
    const DeepQuery path_to_menu_option =
        PathToTopmostItemElement(menu_item_selector);
    StateChange menu_item_visible;
    menu_item_visible.type = StateChange::Type::kExists;
    menu_item_visible.where = path_to_menu_option;
    menu_item_visible.event = kReadyEvent;
    return Steps(
        CheckElementVisible(path_to_menu_button, true),
        ExecuteJsAt(kDownloadsPageTabId, path_to_menu_button, kClickFn),
        WaitForStateChange(kDownloadsPageTabId, menu_item_visible),
        // Use mouse input instead of JavaScript click() to satisfy the
        // user gesture requirement for some menu options.
        MoveMouseTo(kDownloadsPageTabId, path_to_menu_option), ClickMouse());
  }

  // Finds the first download item on the page and clicks a button on the item.
  auto ClickTopmostItemButton(const std::string& button_selector) {
    const DeepQuery path_to_button = PathToTopmostItemElement(button_selector);
    return Steps(CheckElementVisible(path_to_button, true),
                 ExecuteJsAt(kDownloadsPageTabId, path_to_button, kClickFn));
  }

  // Presses the clear all button in the toolbar.
  auto PressClearAll() {
    const DeepQuery kPathToClearAllButton{
        "downloads-manager",
        "downloads-toolbar",
        "#clearAll",
    };
    return ExecuteJsAt(kDownloadsPageTabId, kPathToClearAllButton, kClickFn);
  }

  // Waits for `num` download items to be present in the downloads list. Note
  // that iron-list keeps removed elements in the DOM (for possible reuse) so
  // the number of download items present is not necessarily the number of list
  // elements that are actually displayed, if items have been removed.
  auto WaitForDownloadItems(int num) {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadyEvent);
    const DeepQuery kPathToDownloadsList{
        "downloads-manager",
        "#downloadsList",
    };
    StateChange item_count_matches;
    item_count_matches.type = StateChange::Type::kExistsAndConditionTrue;
    item_count_matches.where = kPathToDownloadsList;
    item_count_matches.test_function = base::StringPrintf(
        "el => el.querySelectorAll('downloads-item').length === %d", num);
    item_count_matches.event = kReadyEvent;
    return WaitForStateChange(kDownloadsPageTabId, item_count_matches);
  }

  // Waits for the "no downloads" splash screen to be displayed.
  auto WaitForNoDownloads() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadyEvent);
    const DeepQuery kPathToNoDownloads{
        "downloads-manager",
        "#no-downloads:not([hidden])",
    };
    StateChange no_downloads_visible;
    no_downloads_visible.type = StateChange::Type::kExists;
    no_downloads_visible.where = kPathToNoDownloads;
    no_downloads_visible.event = kReadyEvent;
    return WaitForStateChange(kDownloadsPageTabId, no_downloads_visible);
  }

  // Downloads a normal file.
  auto DownloadTestFile() {
    return Do(base::BindLambdaForTesting([&]() {
      DownloadAndWait(browser(),
                      embedded_test_server()->GetURL(base::StrCat(
                          {"/", DownloadTestBase::kDownloadTest1Path})));
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitest, DownloadItemsAppear) {
  RunTestSequence(OpenDownloadsPage(),      //
                  DownloadTestFile(),       //
                  WaitForDownloadItems(1),  //
                  DownloadTestFile(),       //
                  WaitForDownloadItems(2));
}

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitest,
                       QuickRemoveDownloadItem) {
  RunTestSequence(
      OpenDownloadsPage(),      //
      DownloadTestFile(),       //
      WaitForDownloadItems(1),  //
      // A normal, completed download should have quick action buttons but not
      // a menu button.
      CheckElementVisible(PathToTopmostItemElement("#more-actions"), false),
      ClickTopmostItemButton("#quick-remove"),  //
      WaitForNoDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitest, ClearAll) {
  RunTestSequence(OpenDownloadsPage(),      //
                  DownloadTestFile(),       //
                  DownloadTestFile(),       //
                  WaitForDownloadItems(2),  //
                  PressClearAll(),          //
                  WaitForNoDownloads());
}

// A test fixture in which a dangerous download returns the template parameter
// danger type.
template <download::DownloadDangerType DangerType>
class DownloadsPageInteractiveUitestWithDangerType
    : public DownloadsPageInteractiveUitest {
 public:
  // This is required to make a download appear to be a malicious danger type.
  // TODO(chlily): Deduplicate from other places similar classes are used.
  class TestDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
   public:
    explicit TestDownloadManagerDelegate(Profile* profile)
        : ChromeDownloadManagerDelegate(profile) {
      if (!profile->IsOffTheRecord()) {
        GetDownloadIdReceiverCallback().Run(download::DownloadItem::kInvalidId +
                                            1);
      }
    }
    ~TestDownloadManagerDelegate() override = default;

    // ChromeDownloadManagerDelegate:
    bool DetermineDownloadTarget(
        download::DownloadItem* item,
        download::DownloadTargetCallback* callback) override {
      auto set_dangerous = [](download::DownloadTargetCallback callback,
                              download::DownloadTargetInfo target_info) {
        target_info.danger_type = DangerType;
        std::move(callback).Run(std::move(target_info));
      };

      download::DownloadTargetCallback dangerous_callback =
          base::BindOnce(set_dangerous, std::move(*callback));
      bool run = ChromeDownloadManagerDelegate::DetermineDownloadTarget(
          item, &dangerous_callback);
      // ChromeDownloadManagerDelegate::DetermineDownloadTarget() needs to run
      // the `callback`.
      CHECK(run);
      CHECK(!dangerous_callback);
      return true;
    }
  };

  DownloadsPageInteractiveUitestWithDangerType() = default;

  void SetUpOnMainThread() override {
    DownloadsPageInteractiveUitest::SetUpOnMainThread();
    auto test_delegate =
        std::make_unique<TestDownloadManagerDelegate>(browser()->profile());
    DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
        ->SetDownloadManagerDelegateForTesting(std::move(test_delegate));
  }

  // Downloads a dangerous file. It's a .swf file so that it's dangerous on all
  // platforms (including CrOS). This .swf normally would be categorized as
  // DANGEROUS_FILE, but TestDownloadManagerDelegate turns it into the template
  // param DangerType.
  auto DownloadDangerousTestFile() {
    return Do(base::BindLambdaForTesting([&]() {
      std::unique_ptr<content::DownloadTestObserver> waiter{
          DangerousDownloadWaiter(
              browser(), /*num_downloads=*/1,
              content::DownloadTestObserver::DangerousDownloadAction::
                  ON_DANGEROUS_DOWNLOAD_QUIT)};
      GURL url =
          embedded_test_server()->GetURL("/downloads/dangerous/dangerous.swf");
      EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
      waiter->WaitForFinished();
    }));
  }

  // Finds the topmost download item and waits for it to be dangerous or not
  // dangerous. If `is_dangerous` is true and `expected_caption_color` is
  // provided, also waits for the caption color to match.
  auto WaitForTopmostItemDanger(
      bool is_dangerous,
      std::optional<std::string> expected_caption_color = std::nullopt) {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadyEvent);
    const DeepQuery path_to_item_danger_caption =
        PathToTopmostItemElement(".description");
    const char kHiddenFn[] = "el => el.getAttribute('hidden') %s null";
    const char kColorMatchCondition[] =
        " && el.getAttribute('description-color') === '%s'";
    // If the download is dangerous, the caption is shown, so the hidden
    // attribute is not present (i.e. equal to null).
    std::string test_function =
        base::StringPrintf(kHiddenFn, is_dangerous ? "===" : "!==");
    if (is_dangerous && expected_caption_color) {
      base::StrAppend(&test_function,
                      {base::StringPrintf(kColorMatchCondition,
                                          expected_caption_color->c_str())});
    }
    StateChange danger_caption_visibility;
    danger_caption_visibility.type = StateChange::Type::kExistsAndConditionTrue;
    danger_caption_visibility.where = path_to_item_danger_caption;
    danger_caption_visibility.test_function = test_function;
    danger_caption_visibility.event = kReadyEvent;
    return WaitForStateChange(kDownloadsPageTabId, danger_caption_visibility);
  }

  auto WaitForBypassWarningPrompt() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadyEvent);
    const DeepQuery kPathToDialog{
        "downloads-manager",
        "downloads-bypass-warning-confirmation-dialog",
    };
    StateChange dialog_visible;
    dialog_visible.type = StateChange::Type::kExists;
    dialog_visible.where = kPathToDialog;
    dialog_visible.event = kReadyEvent;
    return WaitForStateChange(kDownloadsPageTabId, dialog_visible);
  }

  auto ClickBypassWarningPromptButton(const std::string& button_selector) {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadyEvent);
    const DeepQuery path_to_button{
        "downloads-manager",
        "downloads-bypass-warning-confirmation-dialog",
        button_selector,
    };
    StateChange button_visible;
    button_visible.type = StateChange::Type::kExists;
    button_visible.where = path_to_button;
    button_visible.event = kReadyEvent;
    return Steps(WaitForStateChange(kDownloadsPageTabId, button_visible),
                 // Use mouse input instead of JavaScript click() to satisfy the
                 // user gesture requirement for saving a dangerous file.
                 MoveMouseTo(kDownloadsPageTabId, path_to_button),
                 ClickMouse());
  }
};

template <download::DownloadDangerType DangerType>
class DownloadsPageInteractiveUitestWithDangerTypeForBypassDialog
    : public DownloadsPageInteractiveUitestWithDangerType<DangerType> {
 public:
  DownloadsPageInteractiveUitestWithDangerTypeForBypassDialog() {
    feature_list_.InitAndDisableFeature(
        safe_browsing::kDangerousDownloadInterstitial);
  }
  ~DownloadsPageInteractiveUitestWithDangerTypeForBypassDialog() override =
      default;

  void SetUpOnMainThread() override {
    DownloadsPageInteractiveUitestWithDangerType<
        DangerType>::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Uncommon downloads follow the "suspicious" pattern and show up with grey
// icons and text. They can be validated from the page directly without a
// confirmation dialog.
using DownloadsPageInteractiveUitestSuspicious =
    DownloadsPageInteractiveUitestWithDangerType<
        download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT>;

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitestSuspicious,
                       DiscardSuspiciousFile) {
  RunTestSequence(OpenDownloadsPage(),                              //
                  DownloadDangerousTestFile(),                      //
                  WaitForDownloadItems(1),                          //
                  WaitForTopmostItemDanger(true, "grey"),           //
                  TakeTopmostItemMenuAction("#discard-dangerous"),  //
                  WaitForNoDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitestSuspicious,
                       QuickDiscardSuspiciousFile) {
  RunTestSequence(OpenDownloadsPage(),                      //
                  DownloadDangerousTestFile(),              //
                  WaitForDownloadItems(1),                  //
                  WaitForTopmostItemDanger(true, "grey"),   //
                  ClickTopmostItemButton("#quick-remove"),  //
                  WaitForNoDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitestSuspicious,
                       ValidateSuspiciousFile) {
  RunTestSequence(OpenDownloadsPage(),                           //
                  DownloadDangerousTestFile(),                   //
                  WaitForDownloadItems(1),                       //
                  WaitForTopmostItemDanger(true, "grey"),        //
                  TakeTopmostItemMenuAction("#save-dangerous"),  //
                  WaitForTopmostItemDanger(false));
}

// Dangerous downloads follow the "dangerous" pattern and show up with red
// icons and text. They can be validated only from the confirmation dialog.
using DownloadsPageInteractiveUitestDangerous =
    DownloadsPageInteractiveUitestWithDangerType<
        download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT>;

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitestDangerous,
                       DiscardDangerousFile) {
  RunTestSequence(OpenDownloadsPage(),                              //
                  DownloadDangerousTestFile(),                      //
                  WaitForDownloadItems(1),                          //
                  WaitForTopmostItemDanger(true, "red"),            //
                  TakeTopmostItemMenuAction("#discard-dangerous"),  //
                  WaitForNoDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitestDangerous,
                       QuickDiscardDangerousFile) {
  RunTestSequence(OpenDownloadsPage(),                      //
                  DownloadDangerousTestFile(),              //
                  WaitForDownloadItems(1),                  //
                  WaitForTopmostItemDanger(true, "red"),    //
                  ClickTopmostItemButton("#quick-remove"),  //
                  WaitForNoDownloads());
}

// Dangerous downloads follow the "dangerous" pattern and show up with red
// icons and text. They can be validated only from the confirmation dialog.
using DownloadsPageInteractiveUitestDangerousFromBypassDialog =
    DownloadsPageInteractiveUitestWithDangerTypeForBypassDialog<
        download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT>;

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitestDangerousFromBypassDialog,
                       ValidateDangerousFileFromPrompt) {
  RunTestSequence(
      OpenDownloadsPage(),                                           //
      DownloadDangerousTestFile(),                                   //
      WaitForDownloadItems(1),                                       //
      WaitForTopmostItemDanger(true, "red"),                         //
      TakeTopmostItemMenuAction("#save-dangerous"),                  //
      WaitForBypassWarningPrompt(),                                  //
      ClickBypassWarningPromptButton("#download-dangerous-button"),  //
      WaitForTopmostItemDanger(false));
}

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitestDangerousFromBypassDialog,
                       CancelValidateDangerousFile) {
  RunTestSequence(OpenDownloadsPage(),                               //
                  DownloadDangerousTestFile(),                       //
                  WaitForDownloadItems(1),                           //
                  WaitForTopmostItemDanger(true, "red"),             //
                  TakeTopmostItemMenuAction("#save-dangerous"),      //
                  WaitForBypassWarningPrompt(),                      //
                  ClickBypassWarningPromptButton("#cancel-button"),  //
                  WaitForTopmostItemDanger(true, "red"));
}

}  // namespace
