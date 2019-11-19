// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdio.h>

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/history_quick_provider.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/constants/chromeos_features.h"
#endif

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using bookmarks::BookmarkModel;

namespace {

const char kSearchKeyword[] = "foo";
const char kSearchKeyword2[] = "footest.com";
const ui::KeyboardCode kSearchKeywordKeys[] = {
  ui::VKEY_F, ui::VKEY_O, ui::VKEY_O, ui::VKEY_UNKNOWN
};
const ui::KeyboardCode kSearchKeywordPrefixKeys[] = {
  ui::VKEY_F, ui::VKEY_O, ui::VKEY_UNKNOWN
};
const ui::KeyboardCode kSearchKeywordCompletionKeys[] = {
  ui::VKEY_O, ui::VKEY_UNKNOWN
};
const char kSearchURL[] = "http://www.foo.com/search?q={searchTerms}";
const char kSearchShortName[] = "foo";
const char kSearchText[] = "abc";
const ui::KeyboardCode kSearchTextKeys[] = {
  ui::VKEY_A, ui::VKEY_B, ui::VKEY_C, ui::VKEY_UNKNOWN
};
const char kSearchTextURL[] = "http://www.foo.com/search?q=abc";

const char kInlineAutocompleteText[] = "def";
const ui::KeyboardCode kInlineAutocompleteTextKeys[] = {
  ui::VKEY_D, ui::VKEY_E, ui::VKEY_F, ui::VKEY_UNKNOWN
};

// Hostnames that shall be blocked by host resolver.
const char *kBlockedHostnames[] = {
  "foo",
  "*.foo.com",
  "bar",
  "*.bar.com",
  "abc",
  "*.abc.com",
  "def",
  "*.def.com",
  "*.site.com",
  "history",
  "z"
};

const struct TestHistoryEntry {
  const char* url;
  const char* title;
  int visit_count;
  int typed_count;
  bool starred;
} kHistoryEntries[] = {
  {"http://www.bar.com/1", "Page 1", 10, 10, false },
  {"http://www.bar.com/2", "Page 2", 9, 9, false },
  {"http://www.bar.com/3", "Page 3", 8, 8, false },
  {"http://www.bar.com/4", "Page 4", 7, 7, false },
  {"http://www.bar.com/5", "Page 5", 6, 6, false },
  {"http://www.bar.com/6", "Page 6", 5, 5, false },
  {"http://www.bar.com/7", "Page 7", 4, 4, false },
  {"http://www.bar.com/8", "Page 8", 3, 3, false },
  {"http://www.bar.com/9", "Page 9", 2, 2, false },
  {"http://www.site.com/path/1", "Site 1", 4, 4, false },
  {"http://www.site.com/path/2", "Site 2", 3, 3, false },
  {"http://www.site.com/path/3", "Site 3", 2, 2, false },

  // To trigger inline autocomplete.
  {"http://www.def.com", "Page def", 10000, 10000, true },

  // Used in particular for the desired TLD test.  This makes it test
  // the interesting case when there's an intranet host with the same
  // name as the .com.
  {"http://bar/", "Bar", 1, 0, false },
};

// Stores the given text to clipboard.
void SetClipboardText(const base::string16& text) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
  writer.WriteText(text);
}

#if defined(OS_MACOSX)
const int kCtrlOrCmdMask = ui::EF_COMMAND_DOWN;
#else
const int kCtrlOrCmdMask = ui::EF_CONTROL_DOWN;
#endif

}  // namespace

class OmniboxViewTest : public InProcessBrowserTest {
 public:
  OmniboxViewTest() {}

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    ASSERT_NO_FATAL_FAILURE(SetupComponents());
    chrome::FocusLocationBar(browser());
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  }

  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

  static void GetOmniboxViewForBrowser(
      const Browser* browser,
      OmniboxView** omnibox_view) {
    BrowserWindow* window = browser->window();
    ASSERT_TRUE(window);
    LocationBar* location_bar = window->GetLocationBar();
    ASSERT_TRUE(location_bar);
    *omnibox_view = location_bar->GetOmniboxView();
    ASSERT_TRUE(*omnibox_view);
  }

  void GetOmniboxView(OmniboxView** omnibox_view) {
    GetOmniboxViewForBrowser(browser(), omnibox_view);
  }

  static void SendKeyForBrowser(const Browser* browser,
                                ui::KeyboardCode key,
                                int modifiers) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser, key,
        (modifiers & ui::EF_CONTROL_DOWN) != 0,
        (modifiers & ui::EF_SHIFT_DOWN) != 0,
        (modifiers & ui::EF_ALT_DOWN) != 0,
        (modifiers & ui::EF_COMMAND_DOWN) != 0));
  }

  void SendKey(ui::KeyboardCode key, int modifiers) {
    SendKeyForBrowser(browser(), key, modifiers);
  }

  void SendKeySequence(const ui::KeyboardCode* keys) {
    for (; *keys != ui::VKEY_UNKNOWN; ++keys)
      ASSERT_NO_FATAL_FAILURE(SendKey(*keys, 0));
  }

  void ExpectBrowserClosed(Browser* browser,
                           ui::KeyboardCode key,
                           int modifiers) {
    // Press the accelerator after starting to wait for a browser to close as
    // the close may be synchronous.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const Browser* browser, ui::KeyboardCode key, int modifiers) {
              EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
                  browser, key, (modifiers & ui::EF_CONTROL_DOWN) != 0,
                  (modifiers & ui::EF_SHIFT_DOWN) != 0,
                  (modifiers & ui::EF_ALT_DOWN) != 0,
                  (modifiers & ui::EF_COMMAND_DOWN) != 0));
            },
            browser, key, modifiers));
    ui_test_utils::WaitForBrowserToClose(browser);
  }

  void NavigateExpectUrl(const GURL& url, int modifiers = 0) {
    content::TestNavigationObserver observer(url);
    observer.WatchExistingWebContents();
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, modifiers));
    observer.WaitForNavigationFinished();
  }

  void WaitForTabOpenOrClose(int expected_tab_count) {
    int tab_count = browser()->tab_strip_model()->count();
    if (tab_count == expected_tab_count)
      return;

    while (!HasFailure() &&
           browser()->tab_strip_model()->count() != expected_tab_count) {
      content::RunMessageLoop();
    }

    ASSERT_EQ(expected_tab_count, browser()->tab_strip_model()->count());
  }

  void WaitForAutocompleteControllerDone() {
    OmniboxView* omnibox_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

    AutocompleteController* controller =
        omnibox_view->model()->autocomplete_controller();
    ASSERT_TRUE(controller);

    if (controller->done())
      return;

    ui_test_utils::WaitForAutocompleteDone(browser());
    ASSERT_TRUE(controller->done());
  }

  void SetupSearchEngine() {
    Profile* profile = browser()->profile();
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(profile);
    ASSERT_TRUE(model);

    search_test_utils::WaitForTemplateURLServiceToLoad(model);

    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.SetShortName(ASCIIToUTF16(kSearchShortName));
    data.SetKeyword(ASCIIToUTF16(kSearchKeyword));
    data.SetURL(kSearchURL);
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    model->SetUserSelectedDefaultSearchProvider(template_url);

    data.SetKeyword(ASCIIToUTF16(kSearchKeyword2));
    model->Add(std::make_unique<TemplateURL>(data));

    // Remove built-in template urls, like google.com, bing.com etc., as they
    // may appear as autocomplete suggests and interfere with our tests.
    TemplateURLService::TemplateURLVector urls = model->GetTemplateURLs();
    for (TemplateURLService::TemplateURLVector::const_iterator i = urls.begin();
         i != urls.end();
         ++i) {
      if ((*i)->prepopulate_id() != 0)
        model->Remove(*i);
    }
  }

  void AddHistoryEntry(const TestHistoryEntry& entry, const base::Time& time) {
    Profile* profile = browser()->profile();
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service);

    if (!history_service->BackendLoaded()) {
      // Running the task scheduler until idle loads the history backend.
      content::RunAllTasksUntilIdle();
      ASSERT_TRUE(history_service->BackendLoaded());
    }

    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile);
    ASSERT_TRUE(bookmark_model);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    GURL url(entry.url);
    // Add everything in order of time. We don't want to have a time that
    // is "right now" or it will nondeterministically appear in the results.
    history_service->AddPageWithDetails(url, base::UTF8ToUTF16(entry.title),
                                        entry.visit_count,
                                        entry.typed_count, time, false,
                                        history::SOURCE_BROWSED);
    if (entry.starred)
      bookmarks::AddIfNotBookmarked(bookmark_model, url, base::string16());

    // Running the task scheduler until idle finishes AddPageWithDetails.
    content::RunAllTasksUntilIdle();
  }

  void SetupHistory() {
    // Add enough history pages containing |kSearchText| to trigger
    // open history page url in autocomplete result.
    for (size_t i = 0; i < base::size(kHistoryEntries); i++) {
      // Add everything in order of time. We don't want to have a time that
      // is "right now" or it will nondeterministically appear in the results.
      base::Time t = base::Time::Now() - base::TimeDelta::FromHours(i + 1);
      ASSERT_NO_FATAL_FAILURE(AddHistoryEntry(kHistoryEntries[i], t));
    }
  }

  void SetupHostResolver() {
    for (size_t i = 0; i < base::size(kBlockedHostnames); ++i)
      host_resolver()->AddSimulatedFailure(kBlockedHostnames[i]);
  }

  void SetupComponents() {
    ASSERT_NO_FATAL_FAILURE(SetupHostResolver());
    ASSERT_NO_FATAL_FAILURE(SetupSearchEngine());
    ASSERT_NO_FATAL_FAILURE(SetupHistory());
  }

  void SetTestToolbarPermanentText(const base::string16& text) {
    OmniboxView* omnibox_view = nullptr;
    ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
    OmniboxEditModel* edit_model = omnibox_view->model();
    ASSERT_NE(nullptr, edit_model);

    if (!test_location_bar_model_) {
      test_location_bar_model_ = new TestLocationBarModel;
      std::unique_ptr<LocationBarModel> location_bar_model(
          test_location_bar_model_);
      browser()->swap_location_bar_models(&location_bar_model);
    }

    test_location_bar_model_->set_formatted_full_url(text);

    // Normally the URL for display has portions elided. We aren't doing that in
    // this case, because that is irrevelant for these tests.
    test_location_bar_model_->set_url_for_display(text);

    omnibox_view->Update();
  }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;

  // Non-owning pointer.
  TestLocationBarModel* test_location_bar_model_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewTest);
};

// Test if ctrl-* accelerators are workable in omnibox.
// Flaky. See https://crbug.com/751031.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DISABLED_BrowserAccelerators) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  int tab_count = browser()->tab_strip_model()->count();

  // Create a new Tab.
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count + 1));

  // Select the first Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_1, kCtrlOrCmdMask));
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  chrome::FocusLocationBar(browser());

  // Select the second Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_2, kCtrlOrCmdMask));
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  chrome::FocusLocationBar(browser());

  // Try ctrl-w to close a Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_W, kCtrlOrCmdMask));
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count));

  // Try ctrl-l to focus location bar.
  omnibox_view->SetUserText(ASCIIToUTF16("Hello world"));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_L, kCtrlOrCmdMask));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Try editing the location bar text.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, 0));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_S, 0));
  EXPECT_EQ(ASCIIToUTF16("Hello worlds"), omnibox_view->GetText());

  // Try ctrl-x to cut text.
#if defined(OS_MACOSX)
  // Mac uses alt-left/right to select a word.
  ASSERT_NO_FATAL_FAILURE(
      SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
#else
  ASSERT_NO_FATAL_FAILURE(
      SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN));
#endif
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_X, kCtrlOrCmdMask));
  EXPECT_EQ(ASCIIToUTF16("Hello "), omnibox_view->GetText());

#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
  // Try alt-f4 to close the browser.
  ExpectBrowserClosed(browser(), ui::VKEY_F4, ui::EF_ALT_DOWN);
#endif
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, PopupAccelerators) {
  // Create a popup.
  Browser* popup = CreateBrowserForPopup(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(popup));
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(
      GetOmniboxViewForBrowser(popup, &omnibox_view));
  chrome::FocusLocationBar(popup);
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Try ctrl/cmd-w to close the popup.
  ExpectBrowserClosed(popup, ui::VKEY_W, kCtrlOrCmdMask);

  // Create another popup.
  popup = CreateBrowserForPopup(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(popup));
  ASSERT_NO_FATAL_FAILURE(
      GetOmniboxViewForBrowser(popup, &omnibox_view));

  // Set the edit text to "Hello world".
  omnibox_view->SetUserText(ASCIIToUTF16("Hello world"));
  chrome::FocusLocationBar(popup);
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Try editing the location bar text -- should be disallowed.
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_S, 0));
  EXPECT_EQ(ASCIIToUTF16("Hello world"), omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  ASSERT_NO_FATAL_FAILURE(
      SendKeyForBrowser(popup, ui::VKEY_X, kCtrlOrCmdMask));
  EXPECT_EQ(ASCIIToUTF16("Hello world"), omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());

#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
  // Try alt-f4 to close the popup.
  ExpectBrowserClosed(popup, ui::VKEY_F4, ui::EF_ALT_DOWN);
#endif
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, BackspaceInKeywordMode) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));

  // Backspace without search text should bring back keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));

  // Trigger keyword mode again.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Should stay in keyword mode while deleting search text by pressing
  // backspace.
  for (size_t i = 0; i < base::size(kSearchText) - 1; ++i) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
    ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));
  }

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Move cursor to the beginning of the search text.
#if defined(OS_MACOSX)
  // Home doesn't work on Mac trybot.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN));
#else
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_HOME, 0));
#endif
  // Backspace at the beginning of the search text shall turn off
  // the keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(base::string16(), omnibox_view->model()->keyword());
  ASSERT_EQ(std::string(kSearchKeyword) + ' ' + kSearchText,
            UTF16ToUTF8(omnibox_view->GetText()));
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DesiredTLD) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Test ctrl-Enter.
  const ui::KeyboardCode kKeys[] = {
    ui::VKEY_B, ui::VKEY_A, ui::VKEY_R, ui::VKEY_UNKNOWN
  };
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  // ctrl-Enter triggers desired_tld feature, thus www.bar.com shall be
  // opened.
  ASSERT_NO_FATAL_FAILURE(
      NavigateExpectUrl(GURL("http://www.bar.com/"), ui::EF_CONTROL_DOWN));
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DesiredTLDWithTemporaryText) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  Profile* profile = browser()->profile();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  // Add a non-substituting keyword. This ensures the popup will have a
  // non-verbatim entry with "ab" as a prefix. This way, by arrowing down, we
  // can set "abc" as temporary text in the omnibox.
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("abc"));
  data.SetKeyword(ASCIIToUTF16(kSearchText));
  data.SetURL("http://abc.com/");
  template_url_service->Add(std::make_unique<TemplateURL>(data));

  // Send "ab", so that an "abc" entry appears in the popup.
  const ui::KeyboardCode kSearchTextPrefixKeys[] = {
    ui::VKEY_A, ui::VKEY_B, ui::VKEY_UNKNOWN
  };
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextPrefixKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  // Arrow down to the "abc" entry in the popup.
  size_t size = popup_model->result().size();
  while (popup_model->selected_line() < size - 1) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, 0));
    if (omnibox_view->GetText() == ASCIIToUTF16("abc"))
      break;
  }
  ASSERT_EQ(ASCIIToUTF16("abc"), omnibox_view->GetText());

  // Hitting ctrl-enter should navigate based on the current text rather than
  // the original input, i.e. to www.abc.com instead of www.ab.com.
  ASSERT_NO_FATAL_FAILURE(
      NavigateExpectUrl(GURL("http://www.abc.com/"), ui::EF_CONTROL_DOWN));
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, ClearUserTextAfterBackgroundCommit) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Navigate in first tab and enter text into the omnibox.
  GURL url1("data:text/html,page1");
  ui_test_utils::NavigateToURL(browser(), url1);
  omnibox_view->SetUserText(ASCIIToUTF16("foo"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create another tab in the foreground.
  AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate in the first tab, currently in the background.
  GURL url2("data:text/html,page2");
  NavigateParams params(browser(), url2, ui::PAGE_TRANSITION_LINK);
  params.source_contents = contents;
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  ui_test_utils::NavigateToURL(&params);

  // Switch back to the first tab.  The user text should be cleared, and the
  // omnibox should have the new URL.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(ASCIIToUTF16(url2.spec()), omnibox_view->GetText());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, AltEnter) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  omnibox_view->SetUserText(ASCIIToUTF16(chrome::kChromeUIHistoryURL));
  int tab_count = browser()->tab_strip_model()->count();
  // alt-Enter opens a new tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, ui::EF_ALT_DOWN));
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count + 1));
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, EnterToSearch) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Test Enter to search.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            popup_model->result().default_match()->type);
  ASSERT_NO_FATAL_FAILURE(NavigateExpectUrl(GURL(kSearchTextURL)));

  // Test that entering a single character then Enter performs a search.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(omnibox_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, 0));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            popup_model->result().default_match()->type);
  ASSERT_NO_FATAL_FAILURE(
      NavigateExpectUrl(GURL("http://www.foo.com/search?q=z")));
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, EscapeToDefaultMatch) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  base::string16 old_text = omnibox_view->GetText();

  // Make sure inline autocomplete is triggered.
  EXPECT_GT(old_text.length(), base::size(kInlineAutocompleteText) - 1);

  size_t old_selected_line = popup_model->selected_line();
  EXPECT_EQ(0U, old_selected_line);

  // Move to another line with different text.
  size_t size = popup_model->result().size();
  while (popup_model->selected_line() < size - 1) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, 0));
    ASSERT_NE(old_selected_line, popup_model->selected_line());
    if (old_text != omnibox_view->GetText())
      break;
  }

  EXPECT_NE(old_text, omnibox_view->GetText());

  // Escape shall revert back to the default match item.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_ESCAPE, 0));
  EXPECT_EQ(old_text, omnibox_view->GetText());
  EXPECT_EQ(old_selected_line, popup_model->selected_line());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest,
                       RevertDefaultRevertInlineTextWhenSelectingDefaultMatch) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  base::string16 old_text = omnibox_view->GetText();

  // Make sure inline autocomplete is triggered.
  EXPECT_GT(old_text.length(), base::size(kInlineAutocompleteText) - 1);

  size_t old_selected_line = popup_model->selected_line();
  EXPECT_EQ(0U, old_selected_line);

  // Move to another line with different text.
  size_t size = popup_model->result().size();
  while (popup_model->selected_line() < size - 1) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, 0));
    ASSERT_NE(old_selected_line, popup_model->selected_line());
    if (old_text != omnibox_view->GetText())
      break;
  }

  EXPECT_NE(old_text, omnibox_view->GetText());

  // Move back to the first line
  while (popup_model->selected_line() > 0)
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_UP, 0));

  EXPECT_EQ(old_text, omnibox_view->GetText());
  EXPECT_EQ(old_selected_line, popup_model->selected_line());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest,
                       RendererInitiatedFocusPreservesUserText) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Type a single character.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, 0));
  EXPECT_EQ(base::ASCIIToUTF16("a"), omnibox_view->GetText());

  // Simulate a renderer-initated focus event.
  browser()->SetFocusToLocationBar();

  // Type an additional character and verify that we didn't clobber the
  // character we already typed.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_B, 0));
  EXPECT_EQ(base::ASCIIToUTF16("ab"), omnibox_view->GetText());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, BasicTextOperations) {
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  chrome::FocusLocationBar(browser());

  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  base::string16 old_text = omnibox_view->GetText();
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), old_text);
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  size_t start, end;
  omnibox_view->GetSelectionBounds(&start, &end);
#if defined(TOOLKIT_VIEWS)
  // Views textfields select-all in reverse to show the leading text.
  std::swap(start, end);
#endif
  EXPECT_EQ(0U, start);
  EXPECT_EQ(old_text.size(), end);

  // Move the cursor to the end.
#if defined(OS_MACOSX)
  // End doesn't work on Mac trybot.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_E, ui::EF_CONTROL_DOWN));
#else
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, 0));
#endif
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Make sure the cursor is placed correctly.
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(old_text.size(), start);
  EXPECT_EQ(old_text.size(), end);

  // Insert one character at the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, 0));
  EXPECT_EQ(old_text + base::char16('a'), omnibox_view->GetText());

  // Delete one character from the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  EXPECT_EQ(old_text, omnibox_view->GetText());

  omnibox_view->SelectAll(true);
  EXPECT_TRUE(omnibox_view->IsSelectAll());
  omnibox_view->GetSelectionBounds(&start, &end);
#if defined(TOOLKIT_VIEWS)
  // Views textfields select-all in reverse to show the leading text.
  std::swap(start, end);
#endif
  EXPECT_EQ(0U, start);
  EXPECT_EQ(old_text.size(), end);

  // Delete the content
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DELETE, 0));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(0U, end);
  EXPECT_TRUE(omnibox_view->GetText().empty());

  // Add a small amount of text to move the cursor past offset 0.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_B, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_C, 0));

  // Check if RevertAll() resets the text and preserves the cursor position.
  omnibox_view->RevertAll();
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  EXPECT_EQ(old_text, omnibox_view->GetText());
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(3U, start);
  EXPECT_EQ(3U, end);

  // Check that reverting clamps the cursor to the bounds of the new text.
  // Move the cursor to the end.
#if defined(OS_MACOSX)
  // End doesn't work on Mac trybot.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_E, ui::EF_CONTROL_DOWN));
#else
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, 0));
#endif
  // Add a small amount of text to push the cursor past where the text end
  // will be once we revert.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, 0));
  omnibox_view->RevertAll();
  // Cursor should be no further than original text.
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(11U, start);
  EXPECT_EQ(11U, end);
}

// Make sure the cursor position doesn't get set past the last character of
// user input text when the URL is longer than the keyword.
// (http://crbug.com/656209)
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, FocusSearchLongUrl) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  ASSERT_GT(strlen(url::kAboutBlankURL), strlen(kSearchKeyword));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Make sure nothing DCHECKs.
  chrome::FocusSearch(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, AcceptKeywordByTypingQuestionMark) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  base::string16 search_keyword(ASCIIToUTF16(kSearchKeyword));

  // If the user gets into keyword mode by typing '?', they should be put into
  // keyword mode for their default search provider.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_TRUE(omnibox_view->model()->is_keyword_selected());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(base::string16(), omnibox_view->GetText());

  // If the user press backspace, they should be left with '?' in the omnibox.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  EXPECT_EQ(base::ASCIIToUTF16("?"), omnibox_view->GetText());
  EXPECT_EQ(base::string16(), omnibox_view->model()->keyword());
  EXPECT_FALSE(omnibox_view->model()->is_keyword_hint());
  EXPECT_FALSE(omnibox_view->model()->is_keyword_selected());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, SearchDisabledDontCrashOnQuestionMark) {
  policy::PolicyMap policies;
  policies.Set("DefaultSearchProviderEnabled", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
               std::make_unique<base::Value>(false), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // '?' isn't in the same place on all keyboard layouts, so send the character
  // instead of keystrokes.
  ASSERT_NO_FATAL_FAILURE({
    omnibox_view->OnBeforePossibleChange();
    omnibox_view->SetUserText(base::UTF8ToUTF16("?"));
    omnibox_view->OnAfterPossibleChange(true);
  });
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_FALSE(omnibox_view->model()->is_keyword_selected());
  ASSERT_EQ(ASCIIToUTF16("?"), omnibox_view->GetText());
}

// Flaky on TSAN. https://crbug.com/911614
#if defined(THREAD_SANITIZER)
#define MAYBE_AcceptKeywordBySpace DISABLED_AcceptKeywordBySpace
#else
#define MAYBE_AcceptKeywordBySpace AcceptKeywordBySpace
#endif
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, MAYBE_AcceptKeywordBySpace) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  base::string16 search_keyword(ASCIIToUTF16(kSearchKeyword));

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(search_keyword, omnibox_view->GetText());

  // Trigger keyword mode by space.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_TRUE(omnibox_view->GetText().empty());
  size_t start, end;
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(0U, end);

  // Revert to keyword hint mode.
  omnibox_view->model()->ClearKeyword();
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(search_keyword + base::char16(' '), omnibox_view->GetText());
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(search_keyword.length() + 1, start);
  EXPECT_EQ(search_keyword.length() + 1, end);

  // Keyword should also be accepted by typing an ideographic space.
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetWindowTextAndCaretPos(search_keyword +
      base::WideToUTF16(L"\x3000"), search_keyword.length() + 1, false, false);
  omnibox_view->OnAfterPossibleChange(true);
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_TRUE(omnibox_view->GetText().empty());

  // Revert to keyword hint mode.
  omnibox_view->model()->ClearKeyword();
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(search_keyword + base::char16(' '), omnibox_view->GetText());

  // Keyword shouldn't be accepted by pressing space with a trailing
  // whitespace.
  omnibox_view->SetWindowTextAndCaretPos(search_keyword + base::char16(' '),
      search_keyword.length() + 1, false, false);
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(search_keyword + ASCIIToUTF16("  "), omnibox_view->GetText());

  // Keyword shouldn't be accepted by deleting the trailing space.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(search_keyword + base::char16(' '), omnibox_view->GetText());

  // Keyword shouldn't be accepted by pressing space before a trailing space.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(search_keyword + ASCIIToUTF16("  "), omnibox_view->GetText());

  // Keyword should be accepted by pressing space in the middle of context and
  // just after the keyword.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(ASCIIToUTF16("a "), omnibox_view->GetText());
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(0U, end);

  // Keyword shouldn't be accepted by pasting "foo bar".
  omnibox_view->SetUserText(base::string16());
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_TRUE(omnibox_view->model()->keyword().empty());

  omnibox_view->OnBeforePossibleChange();
  omnibox_view->model()->OnPaste();
  omnibox_view->SetWindowTextAndCaretPos(search_keyword +
      ASCIIToUTF16(" bar"), search_keyword.length() + 4, false, false);
  omnibox_view->OnAfterPossibleChange(true);
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_TRUE(omnibox_view->model()->keyword().empty());
  ASSERT_EQ(search_keyword + ASCIIToUTF16(" bar"), omnibox_view->GetText());

  // Keyword shouldn't be accepted for case like: "foo b|ar" -> "foo b |ar".
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_TRUE(omnibox_view->model()->keyword().empty());
  ASSERT_EQ(search_keyword + ASCIIToUTF16(" b ar"), omnibox_view->GetText());

  // Keyword could be accepted by pressing space with a selected range at the
  // end of text.
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->OnInlineAutocompleteTextMaybeChanged(
      search_keyword + ASCIIToUTF16("  "), search_keyword.length());
  omnibox_view->OnAfterPossibleChange(true);
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(search_keyword + ASCIIToUTF16("  "), omnibox_view->GetText());

  omnibox_view->GetSelectionBounds(&start, &end);
  ASSERT_NE(start, end);
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(base::string16(), omnibox_view->GetText());

  // Space should accept keyword even when inline autocomplete is available.
  omnibox_view->SetUserText(base::string16());
  const TestHistoryEntry kHistoryFoobar = {
    "http://www.foobar.com", "Page foobar", 100, 100, true
  };

  // Add a history entry to trigger inline autocomplete when typing "foo".
  ASSERT_NO_FATAL_FAILURE(AddHistoryEntry(
      kHistoryFoobar, base::Time::Now() - base::TimeDelta::FromHours(1)));

  // Type "fo" to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordPrefixKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->popup_model()->IsOpen());
  ASSERT_NE(search_keyword, omnibox_view->GetText());

  // Keyword hint shouldn't be visible.
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_TRUE(omnibox_view->model()->keyword().empty());

  // Add the "o".  Inline autocompletion should still happen, but now we
  // should also get a keyword hint because we've typed a keyword exactly.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordCompletionKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->popup_model()->IsOpen());
  ASSERT_NE(search_keyword, omnibox_view->GetText());
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_FALSE(omnibox_view->model()->keyword().empty());

  // Trigger keyword mode by space.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_TRUE(omnibox_view->GetText().empty());

  // Space in the middle of a temporary text, which separates the text into
  // keyword and replacement portions, should trigger keyword mode.
  omnibox_view->SetUserText(base::string16());
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model->IsOpen());
  ASSERT_EQ(ASCIIToUTF16("foobar.com"), omnibox_view->GetText());
  omnibox_view->model()->OnUpOrDownKeyPressed(1);
  omnibox_view->model()->OnUpOrDownKeyPressed(-1);
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(ASCIIToUTF16("bar.com"), omnibox_view->GetText());

  // Space after temporary text that looks like a keyword, when the original
  // input does not look like a keyword, should trigger keyword mode.
  omnibox_view->SetUserText(base::string16());
  const TestHistoryEntry kHistoryFoo = {
    "http://footest.com", "Page footest", 1000, 1000, true
  };

  // Add a history entry to trigger HQP matching with text == keyword when
  // typing "fo te".
  ASSERT_NO_FATAL_FAILURE(AddHistoryEntry(
      kHistoryFoo, base::Time::Now() - base::TimeDelta::FromMinutes(10)));

  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_F, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_O, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_T, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_E, 0));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  base::string16 search_keyword2(ASCIIToUTF16(kSearchKeyword2));
  while ((omnibox_view->GetText() != search_keyword2) &&
         (popup_model->selected_line() < popup_model->result().size() - 1))
    omnibox_view->model()->OnUpOrDownKeyPressed(1);
  ASSERT_EQ(search_keyword2, omnibox_view->GetText());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(search_keyword2, omnibox_view->model()->keyword());
  ASSERT_TRUE(omnibox_view->GetText().empty());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, NonSubstitutingKeywordTest) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  Profile* profile = browser()->profile();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  // Add a non-default substituting keyword.
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("Search abc"));
  data.SetKeyword(ASCIIToUTF16(kSearchText));
  data.SetURL("http://abc.com/{searchTerms}");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));

  omnibox_view->SetUserText(base::string16());

  // Non-default substituting keyword shouldn't be matched by default.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  // Check if the default match result is Search Primary Provider.
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            popup_model->result().default_match()->type);
  ASSERT_EQ(kSearchTextURL,
            popup_model->result().default_match()->destination_url.spec());

  omnibox_view->SetUserText(base::string16());
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_FALSE(popup_model->IsOpen());

  // Try a non-substituting keyword.
  template_url_service->Remove(template_url);
  data.SetShortName(ASCIIToUTF16("abc"));
  data.SetURL("http://abc.com/");
  template_url_service->Add(std::make_unique<TemplateURL>(data));

  // We always allow exact matches for non-substituting keywords.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  ASSERT_EQ(AutocompleteMatchType::HISTORY_KEYWORD,
            popup_model->result().default_match()->type);
  ASSERT_EQ("http://abc.com/",
            popup_model->result().default_match()->destination_url.spec());
}

// Flaky. See https://crbug.com/751031.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DISABLED_DeleteItem) {
  // Disable the search provider, to make sure the popup contains only history
  // items.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  model->SetUserSelectedDefaultSearchProvider(NULL);

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  chrome::FocusLocationBar(browser());

  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  base::string16 old_text = omnibox_view->GetText();

  // Input something that can match history items.
  omnibox_view->SetUserText(ASCIIToUTF16("site.com/p"));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  // Delete the inline autocomplete part.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DELETE, 0));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  ASSERT_GE(popup_model->result().size(), 3U);

  base::string16 user_text = omnibox_view->GetText();
  ASSERT_EQ(ASCIIToUTF16("site.com/p"), user_text);
  omnibox_view->SelectAll(true);
  ASSERT_TRUE(omnibox_view->IsSelectAll());

  // Move down.
  size_t default_line = popup_model->selected_line();
  omnibox_view->model()->OnUpOrDownKeyPressed(1);
  ASSERT_EQ(default_line + 1, popup_model->selected_line());
  base::string16 selected_text =
      popup_model->result().match_at(default_line + 1).fill_into_edit;
  // Temporary text is shown.
  ASSERT_EQ(selected_text, omnibox_view->GetText());
  ASSERT_FALSE(omnibox_view->IsSelectAll());

  // Delete the item.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DELETE, ui::EF_SHIFT_DOWN));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  // The selected line shouldn't be changed, because we have more than two
  // items.
  ASSERT_EQ(default_line + 1, popup_model->selected_line());
  // Make sure the item is really deleted.
  ASSERT_NE(selected_text,
            popup_model->result().match_at(default_line + 1).fill_into_edit);
  selected_text =
      popup_model->result().match_at(default_line + 1).fill_into_edit;
  // New temporary text is shown.
  ASSERT_EQ(selected_text, omnibox_view->GetText());

  // Revert to the default match.
  ASSERT_TRUE(omnibox_view->model()->OnEscapeKeyPressed());
  ASSERT_EQ(default_line, popup_model->selected_line());
  ASSERT_EQ(user_text, omnibox_view->GetText());
  ASSERT_TRUE(omnibox_view->IsSelectAll());

  // Move down and up to select the default match as temporary text.
  omnibox_view->model()->OnUpOrDownKeyPressed(1);
  ASSERT_EQ(default_line + 1, popup_model->selected_line());
  omnibox_view->model()->OnUpOrDownKeyPressed(-1);
  ASSERT_EQ(default_line, popup_model->selected_line());

  selected_text = popup_model->result().match_at(default_line).fill_into_edit;
  // New temporary text is shown.
  ASSERT_EQ(selected_text, omnibox_view->GetText());
  ASSERT_FALSE(omnibox_view->IsSelectAll());

#if 0
  // TODO(mrossetti): http://crbug.com/82335
  // Delete the default item.
  popup_model->TryDeletingLine(popup_model->selected_line());
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  // The selected line shouldn't be changed, but the default item should have
  // been changed.
  ASSERT_EQ(default_line, popup_model->selected_line());
  // Make sure the item is really deleted.
  EXPECT_NE(selected_text,
            popup_model->result().match_at(default_line).fill_into_edit);
  selected_text =
      popup_model->result().match_at(default_line).fill_into_edit;
  // New temporary text is shown.
  ASSERT_EQ(selected_text, omnibox_view->GetText());
#endif

  // As the current selected item is the new default item, pressing Escape key
  // should revert all directly.
  ASSERT_TRUE(omnibox_view->model()->OnEscapeKeyPressed());
  ASSERT_EQ(old_text, omnibox_view->GetText());
  ASSERT_TRUE(omnibox_view->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, TabAcceptKeyword) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  base::string16 text = ASCIIToUTF16(kSearchKeyword);

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(text, omnibox_view->model()->keyword());
  ASSERT_EQ(text, omnibox_view->GetText());

  // Trigger keyword mode by tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(text, omnibox_view->model()->keyword());
  ASSERT_TRUE(omnibox_view->GetText().empty());

  // Revert to keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(text, omnibox_view->model()->keyword());
  ASSERT_EQ(text, omnibox_view->GetText());

  // The location bar should still have focus.
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Trigger keyword mode by tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(text, omnibox_view->model()->keyword());
  ASSERT_TRUE(omnibox_view->GetText().empty());

  // Revert to keyword hint mode with SHIFT+TAB.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(text, omnibox_view->model()->keyword());
  ASSERT_EQ(text, omnibox_view->GetText());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}

#if !defined(OS_MACOSX)
// Mac intentionally does not support this behavior.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, WrappingTabTraverseResultsTest) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Input something to trigger results.
  const ui::KeyboardCode kKeys[] = {ui::VKEY_B, ui::VKEY_A, ui::VKEY_R,
                                    ui::VKEY_UNKNOWN};
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  size_t old_selected_line = popup_model->selected_line();
  EXPECT_EQ(0U, old_selected_line);

  // Move down the results.
  for (size_t size = popup_model->result().size();
       popup_model->selected_line() < size - 1;
       old_selected_line = popup_model->selected_line()) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
    ASSERT_LT(old_selected_line, popup_model->selected_line());
  }

  // Wrap to top.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_EQ(0U, popup_model->selected_line());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Wrap to bottom.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN));
  ASSERT_EQ(old_selected_line, popup_model->selected_line());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Move back up the results.
  for (; popup_model->selected_line() > 0U;
       old_selected_line = popup_model->selected_line()) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN));
    ASSERT_GT(old_selected_line, popup_model->selected_line());
  }

  const TestHistoryEntry kHistoryFoo = {"http://foo/", "Page foo", 1, 1, false};

  // Add a history entry so "foo" gets multiple matches.
  ASSERT_NO_FATAL_FAILURE(AddHistoryEntry(
      kHistoryFoo, base::Time::Now() - base::TimeDelta::FromHours(1)));

  // Load results.
  ASSERT_NO_FATAL_FAILURE(omnibox_view->SelectAll(false));
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());

  // Trigger keyword mode by tab.
  base::string16 text = ASCIIToUTF16(kSearchKeyword);
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(text, omnibox_view->model()->keyword());
  ASSERT_TRUE(omnibox_view->GetText().empty());

  // The location bar should still have focus.
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Pressing tab again should move to the next result and clear keyword
  // mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_EQ(1U, omnibox_view->model()->popup_model()->selected_line());
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_NE(text, omnibox_view->model()->keyword());

  // The location bar should still have focus.
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Moving back up should not show keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(text, omnibox_view->model()->keyword());

  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}
#endif

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, PersistKeywordModeOnTabSwitch) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));

  // Create a new tab.
  chrome::NewTab(browser());

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});

  // Make sure we're still in keyword mode.
  ASSERT_TRUE(omnibox_view->model()->is_keyword_selected());
  ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));
  ASSERT_EQ(omnibox_view->GetText(), base::string16());

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Switch to the second tab and back to the first.
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});

  // Make sure we're still in keyword mode.
  ASSERT_TRUE(omnibox_view->model()->is_keyword_selected());
  ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(omnibox_view->model()->keyword()));
  ASSERT_EQ(omnibox_view->GetText(), base::ASCIIToUTF16(kSearchText));
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest,
                       CtrlKeyPressedWithInlineAutocompleteTest) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  base::string16 old_text = omnibox_view->GetText();

  // Make sure inline autocomplete is triggered.
  EXPECT_GT(old_text.length(), base::size(kInlineAutocompleteText) - 1);

  // Press ctrl key.
  omnibox_view->model()->OnControlKeyChanged(true);

  // Inline autocomplete should still be there.
  EXPECT_EQ(old_text, omnibox_view->GetText());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, UndoRedo) {
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  chrome::FocusLocationBar(browser());

  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  base::string16 old_text = omnibox_view->GetText();
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), old_text);
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Delete the text, then undo.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  EXPECT_TRUE(omnibox_view->GetText().empty());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, kCtrlOrCmdMask));
  EXPECT_EQ(old_text, omnibox_view->GetText());

  // Redo should delete the text again.
  ASSERT_NO_FATAL_FAILURE(
      SendKey(ui::VKEY_Z, kCtrlOrCmdMask | ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(omnibox_view->GetText().empty());

  // Perform an undo.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, kCtrlOrCmdMask));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // The text should be selected.
  size_t start, end;
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(old_text.size(), start);
  EXPECT_EQ(0U, end);

  // Delete three characters; "about:bl" should not trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  EXPECT_EQ(old_text.substr(0, old_text.size() - 3), omnibox_view->GetText());

  // Undo delete.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, kCtrlOrCmdMask));
  EXPECT_EQ(old_text, omnibox_view->GetText());

  // Redo delete.
  ASSERT_NO_FATAL_FAILURE(
      SendKey(ui::VKEY_Z, kCtrlOrCmdMask | ui::EF_SHIFT_DOWN));
  EXPECT_EQ(old_text.substr(0, old_text.size() - 3), omnibox_view->GetText());

  // Delete everything.
  omnibox_view->SelectAll(true);
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  EXPECT_TRUE(omnibox_view->GetText().empty());

  // Undo delete everything.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, kCtrlOrCmdMask));
  EXPECT_EQ(old_text.substr(0, old_text.size() - 3), omnibox_view->GetText());

  // Undo delete two characters.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, kCtrlOrCmdMask));
  EXPECT_EQ(old_text, omnibox_view->GetText());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, BackspaceDeleteHalfWidthKatakana) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  // Insert text: ﾀﾞ. This is two, 3-byte UTF-8 characters:
  // U+FF80 "HALFWIDTH KATAKANA LETTER TA" and
  // U+FF9E "HALFWIDTH KATAKANA VOICED SOUND MARK".
  omnibox_view->SetUserText(base::UTF8ToUTF16("\357\276\200\357\276\236"));
  EXPECT_FALSE(omnibox_view->GetText().empty());

  // Move the cursor to the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, 0));

  // Backspace should delete the character. In http://crbug.com/192743, the bug
  // was that nothing was deleted.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
#if defined(OS_MACOSX)
  // Cocoa text fields attach the sound mark and delete the whole thing. This
  // behavior should remain on Mac even when using a toolkit-views browser
  // window.
  EXPECT_TRUE(omnibox_view->GetText().empty());
#else
  // Toolkit-views text fields delete just the sound mark.
  EXPECT_EQ(base::UTF8ToUTF16("\357\276\200"), omnibox_view->GetText());
#endif
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DoesNotUpdateAutocompleteOnBlur) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  size_t start, end;
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_TRUE(start != end);
  base::string16 old_autocomplete_text =
      omnibox_view->model()->autocomplete_controller()->input_.text();

  // Unfocus the omnibox. This should close the popup but should not run
  // autocomplete.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  ASSERT_FALSE(popup_model->IsOpen());
  EXPECT_EQ(old_autocomplete_text,
      omnibox_view->model()->autocomplete_controller()->input_.text());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, Paste) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);
  EXPECT_FALSE(popup_model->IsOpen());

  // Paste should yield the expected text and open the popup.
  SetClipboardText(ASCIIToUTF16(kSearchText));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_V, kCtrlOrCmdMask));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  EXPECT_EQ(ASCIIToUTF16(kSearchText), omnibox_view->GetText());
  EXPECT_TRUE(popup_model->IsOpen());

  // Close the popup and select all.
  omnibox_view->CloseOmniboxPopup();
  omnibox_view->SelectAll(false);
  EXPECT_FALSE(popup_model->IsOpen());

  // Pasting the same text again over itself should re-open the popup.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_V, kCtrlOrCmdMask));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  EXPECT_EQ(ASCIIToUTF16(kSearchText), omnibox_view->GetText());
  EXPECT_TRUE(popup_model->IsOpen());
  omnibox_view->CloseOmniboxPopup();
  EXPECT_FALSE(popup_model->IsOpen());

  // Pasting amid text should yield the expected text and re-open the popup.
  omnibox_view->SetWindowTextAndCaretPos(ASCIIToUTF16("abcd"), 2, false, false);
  SetClipboardText(ASCIIToUTF16("123"));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_V, kCtrlOrCmdMask));
  EXPECT_EQ(ASCIIToUTF16("ab123cd"), omnibox_view->GetText());
  EXPECT_TRUE(popup_model->IsOpen());

  // Ctrl/Cmd+Alt+V should not paste.
  ASSERT_NO_FATAL_FAILURE(
      SendKey(ui::VKEY_V, kCtrlOrCmdMask | ui::EF_ALT_DOWN));
  EXPECT_EQ(ASCIIToUTF16("ab123cd"), omnibox_view->GetText());
  // TODO(msw): Test that AltGr+V does not paste.
}

class OmniboxViewTestWithoutSplitSettings : public OmniboxViewTest {
 public:
  OmniboxViewTestWithoutSplitSettings() {
#if defined(OS_CHROMEOS)
    feature_list_.InitAndDisableFeature(chromeos::features::kSplitSettings);
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxViewTestWithoutSplitSettings, EditSearchEngines) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
#if defined(OS_CHROMEOS)
  // Install the Settings App.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  EXPECT_FALSE(
      chrome::SettingsWindowManager::GetInstance()->FindBrowserForProfile(
          browser()->profile()));
#endif
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_EDIT_SEARCH_ENGINES));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(
      chrome::SettingsWindowManager::GetInstance()->FindBrowserForProfile(
          browser()->profile()));
#else
  const std::string target_url =
      std::string(chrome::kChromeUISettingsURL) + chrome::kSearchEnginesSubPage;
  EXPECT_EQ(ASCIIToUTF16(target_url), omnibox_view->GetText());
#endif
  EXPECT_FALSE(omnibox_view->model()->popup_model()->IsOpen());
}

// Flaky test. The below suggestions are in a random order, and the injected
// keys may or may not have registered. Probably https://crbug.com/751031,
// but I believe the whole input mechanism needs to be re-architected.
// What I'd like to see is, after a sequence of keys is injected, we inject
// an artificial input, and, *only* after that input has been registered,
// do we continue.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest,
                       DISABLED_CtrlArrowAfterArrowSuggestions) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Input something to trigger results.
  const ui::KeyboardCode kKeys[] = {
    ui::VKEY_B, ui::VKEY_A, ui::VKEY_R, ui::VKEY_UNKNOWN
  };
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  ASSERT_EQ(ASCIIToUTF16("bar.com/1"), omnibox_view->GetText());

  // Arrow down on a suggestion, and omnibox text should be the suggestion.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, 0));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_EQ(ASCIIToUTF16("www.bar.com/2"), omnibox_view->GetText());

  // Highlight the last 2 words and the omnibox text should not change.
  // Simulating Ctrl-shift-left only once does not seem to highlight anything
  // on Linux.
#if defined(OS_MACOSX)
  // Mac uses alt-left/right to select a word.
  const int modifiers = ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN;
#else
  const int modifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN;
#endif
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, modifiers));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, modifiers));
  ASSERT_EQ(ASCIIToUTF16("www.bar.com/2"), omnibox_view->GetText());
}

namespace {

// Returns the number of characters currently selected in |omnibox_view|.
size_t GetSelectionSize(OmniboxView* omnibox_view) {
  size_t start, end;
  omnibox_view->GetSelectionBounds(&start, &end);
  if (end >= start)
    return end - start;
  return start - end;
}

}  // namespace

// Test that if the Omnibox has focus, and had everything selected before a
// non-user-initiated update, then it retains the selection after the update.
// Flaky. See https://crbug.com/751031.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DISABLED_SelectAllStaysAfterUpdate) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  base::string16 url_a(ASCIIToUTF16("http://www.a.com/"));
  base::string16 url_b(ASCIIToUTF16("http://www.b.com/"));
  chrome::FocusLocationBar(browser());

  SetTestToolbarPermanentText(url_a);
  EXPECT_EQ(url_a, omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Updating while selected should retain SelectAll().
  SetTestToolbarPermanentText(url_b);
  EXPECT_EQ(url_b, omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Select nothing, then update. Should gain SelectAll().
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, 0));
  SetTestToolbarPermanentText(url_a);
  EXPECT_EQ(url_a, omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Test behavior of the "reversed" attribute of OmniboxView::SelectAll().
  SetTestToolbarPermanentText(ASCIIToUTF16("AB"));
  // Should be at beginning. Shift+left should do nothing.
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  SetTestToolbarPermanentText(ASCIIToUTF16("CD"));
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));

  // At the start, so Shift+Left should do nothing.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));

  // And Shift+Right should reduce by one character.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(1u, GetSelectionSize(omnibox_view));

  // No go to start and select all to the right (not reversed).
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, 0));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, ui::EF_SHIFT_DOWN));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, ui::EF_SHIFT_DOWN));
  SetTestToolbarPermanentText(ASCIIToUTF16("AB"));
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));

  // We reverse select all on Update() so shift-left won't do anything.
  // On Cocoa, shift-left will just anchor it on the left.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));

  // And shift-right should reduce by one character.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(1u, GetSelectionSize(omnibox_view));
}
