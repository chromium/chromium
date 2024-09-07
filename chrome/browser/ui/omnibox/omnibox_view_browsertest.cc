// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/omnibox_view.h"

#include <stddef.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/search_engines/enterprise/site_search_policy_handler.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "url/url_features.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using bookmarks::BookmarkModel;

namespace {

const char16_t kSearchKeyword[] = u"foo";
const char16_t kSearchKeyword2[] = u"footest.com";
const char16_t kSiteSearchPolicyKeyword[] = u"work";
const char16_t kSiteSearchPolicyKeywordWithAtPrefix[] = u"@work";
const ui::KeyboardCode kSearchKeywordKeys[] = {ui::VKEY_F, ui::VKEY_O,
                                               ui::VKEY_O, ui::VKEY_UNKNOWN};
const ui::KeyboardCode kSiteSearchPolicyKeywordKeys[] = {
    ui::VKEY_W, ui::VKEY_O, ui::VKEY_R, ui::VKEY_K, ui::VKEY_UNKNOWN};
const char kSearchURL[] = "http://www.foo.com/search?q={searchTerms}";
const char kSiteSearchPolicyURL[] =
    "http://www.work.com/search?q={searchTerms}";
const char16_t kSearchShortName[] = u"foo";
const char16_t kSiteSearchPolicyName[] = u"Work";
const char16_t kSearchText[] = u"abc";
const ui::KeyboardCode kSearchTextKeys[] = {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C,
                                            ui::VKEY_UNKNOWN};
const char kSearchTextURL[] = "http://www.foo.com/search?q=abc";
const char kSiteSearchPolicyTextURL[] = "http://www.work.com/search?q=abc";

const char kInlineAutocompleteText[] = "def";
const ui::KeyboardCode kInlineAutocompleteTextKeys[] = {
    ui::VKEY_D, ui::VKEY_E, ui::VKEY_F, ui::VKEY_UNKNOWN};

// Hostnames that shall be blocked by host resolver.
const char* kBlockedHostnames[] = {
    "foo", "*.foo.com", "bar",        "*.bar.com", "abc", "*.abc.com",
    "def", "*.def.com", "*.site.com", "history",   "z"};

const struct TestHistoryEntry {
  const char* url;
  const char* title;
  int visit_count;
  int typed_count;
  bool starred;
} kHistoryEntries[] = {
    {"http://www.bar.com/1", "Page 1", 10, 10, false},
    {"http://www.bar.com/2", "Page 2", 9, 9, false},
    {"http://www.bar.com/3", "Page 3", 8, 8, false},
    {"http://www.bar.com/4", "Page 4", 7, 7, false},
    {"http://www.bar.com/5", "Page 5", 6, 6, false},
    {"http://www.bar.com/6", "Page 6", 5, 5, false},
    {"http://www.bar.com/7", "Page 7", 4, 4, false},
    {"http://www.bar.com/8", "Page 8", 3, 3, false},
    {"http://www.bar.com/9", "Page 9", 2, 2, false},
    {"http://www.site.com/path/1", "Site 1", 4, 4, false},
    {"http://www.site.com/path/2", "Site 2", 3, 3, false},
    {"http://www.site.com/path/3", "Site 3", 2, 2, false},

    // To trigger inline autocomplete.
    {"http://www.def.com", "Page def", 10000, 10000, true},

    // Used in particular for the desired TLD test.  This makes it test
    // the interesting case when there's an intranet host with the same
    // name as the .com.
    {"http://bar/", "Bar", 1, 0, false},
};

// Stores the given text to clipboard.
void SetClipboardText(const std::u16string& text) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
  writer.WriteText(text);
}

#if BUILDFLAG(IS_MAC)
const int kCtrlOrCmdMask = ui::EF_COMMAND_DOWN;
#else
const int kCtrlOrCmdMask = ui::EF_CONTROL_DOWN;
#endif

}  // namespace

class OmniboxViewTest : public InProcessBrowserTest {
 public:
  OmniboxViewTest() {}

  OmniboxViewTest(const OmniboxViewTest&) = delete;
  OmniboxViewTest& operator=(const OmniboxViewTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    ASSERT_NO_FATAL_FAILURE(SetupComponents());
    chrome::FocusLocationBar(browser());
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  }

  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

  static void GetOmniboxViewForBrowser(const Browser* browser,
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
        browser, key, (modifiers & ui::EF_CONTROL_DOWN) != 0,
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
    OmniboxView* omnibox_view = nullptr;
    ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

    AutocompleteController* controller =
        omnibox_view->controller()->autocomplete_controller();
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
    data.SetShortName(kSearchShortName);
    data.SetKeyword(kSearchKeyword);
    data.SetURL(kSearchURL);
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    model->SetUserSelectedDefaultSearchProvider(template_url);

    data.SetKeyword(kSearchKeyword2);
    model->Add(std::make_unique<TemplateURL>(data));

    // Remove built-in template urls, like google.com, bing.com etc., as they
    // may appear as autocomplete suggests and interfere with our tests.
    TemplateURLService::TemplateURLVector urls = model->GetTemplateURLs();
    for (TemplateURLService::TemplateURLVector::const_iterator i = urls.begin();
         i != urls.end(); ++i) {
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
                                        entry.visit_count, entry.typed_count,
                                        time, false, history::SOURCE_BROWSED);
    if (entry.starred)
      bookmarks::AddIfNotBookmarked(bookmark_model, url, std::u16string());

    // Running the task scheduler until idle finishes AddPageWithDetails.
    content::RunAllTasksUntilIdle();
  }

  void SetupHistory() {
    // Add enough history pages containing |kSearchText| to trigger
    // open history page url in autocomplete result.
    for (size_t i = 0; i < std::size(kHistoryEntries); i++) {
      // Add everything in order of time. We don't want to have a time that
      // is "right now" or it will nondeterministically appear in the results.
      base::Time t = base::Time::Now() - base::Hours(i + 1);
      ASSERT_NO_FATAL_FAILURE(AddHistoryEntry(kHistoryEntries[i], t));
    }
  }

  void SetupHostResolver() {
    for (size_t i = 0; i < std::size(kBlockedHostnames); ++i)
      host_resolver()->AddSimulatedFailure(kBlockedHostnames[i]);
  }

  void SetupComponents() {
    ASSERT_NO_FATAL_FAILURE(SetupHostResolver());
    ASSERT_NO_FATAL_FAILURE(SetupSearchEngine());
    ASSERT_NO_FATAL_FAILURE(SetupHistory());
  }

  void SetTestToolbarPermanentText(const std::u16string& text) {
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
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  // Non-owning pointer.
  raw_ptr<TestLocationBarModel> test_location_bar_model_ = nullptr;
};

// Test if ctrl-* accelerators are workable in omnibox.
// Flaky. See https://crbug.com/751031.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DISABLED_BrowserAccelerators) {
  OmniboxView* omnibox_view = nullptr;
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
  omnibox_view->SetUserText(u"Hello world");
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_L, kCtrlOrCmdMask));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Try editing the location bar text.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, 0));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_S, 0));
  EXPECT_EQ(u"Hello worlds", omnibox_view->GetText());

  // Try ctrl-x to cut text.
#if BUILDFLAG(IS_MAC)
  // Mac uses alt-left/right to select a word.
  ASSERT_NO_FATAL_FAILURE(
      SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
#else
  ASSERT_NO_FATAL_FAILURE(
      SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN));
#endif
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_X, kCtrlOrCmdMask));
  EXPECT_EQ(u"Hello ", omnibox_view->GetText());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MAC)
  // Try alt-f4 to close the browser.
  ExpectBrowserClosed(browser(), ui::VKEY_F4, ui::EF_ALT_DOWN);
#endif
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, PopupAccelerators) {
  // Create a popup.
  Browser* popup = CreateBrowserForPopup(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(popup));
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(popup, &omnibox_view));
  chrome::FocusLocationBar(popup);
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Try ctrl/cmd-w to close the popup.
  ExpectBrowserClosed(popup, ui::VKEY_W, kCtrlOrCmdMask);

  // Create another popup.
  popup = CreateBrowserForPopup(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(popup));
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(popup, &omnibox_view));

  // Set the edit text to "Hello world".
  omnibox_view->SetUserText(u"Hello world");
  chrome::FocusLocationBar(popup);
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Try editing the location bar text -- should be disallowed.
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_S, 0));
  EXPECT_EQ(u"Hello world", omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_X, kCtrlOrCmdMask));
  EXPECT_EQ(u"Hello world", omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MAC)
  // Try alt-f4 to close the popup.
  ExpectBrowserClosed(popup, ui::VKEY_F4, ui::EF_ALT_DOWN);
#endif
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, BackspaceInKeywordMode) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());

  // Backspace without search text should bring back keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());

  // Trigger keyword mode again.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Should stay in keyword mode while deleting search text by pressing
  // backspace.
  for (size_t i = 0; i < std::size(kSearchText) - 1; ++i) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
    ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());
  }

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Move cursor to the beginning of the search text.
#if BUILDFLAG(IS_MAC)
  // Home doesn't work on Mac trybot.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN));
#else
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_HOME, 0));
#endif
  // Backspace at the beginning of the search text shall turn off
  // the keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  // A keyword 'hint'/button will persist as long as the entry begins with a
  // keyword.
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());
}

// TODO(crbug.com/40661918): This test flakily times out.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DISABLED_DesiredTLD) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Test ctrl-Enter.
  const ui::KeyboardCode kKeys[] = {ui::VKEY_B, ui::VKEY_A, ui::VKEY_R,
                                    ui::VKEY_UNKNOWN};
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  // ctrl-Enter triggers desired_tld feature, thus www.bar.com shall be
  // opened.
  ASSERT_NO_FATAL_FAILURE(
      NavigateExpectUrl(GURL("http://www.bar.com/"), ui::EF_CONTROL_DOWN));
}

// TODO(crbug.com/40661918): Test times out on Win and Linux.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DISABLED_DesiredTLDWithTemporaryText) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  Profile* profile = browser()->profile();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  // Add a non-substituting keyword. This ensures the popup will have a
  // non-verbatim entry with "ab" as a prefix. This way, by arrowing down, we
  // can set "abc" as temporary text in the omnibox.
  TemplateURLData data;
  data.SetShortName(u"abc");
  data.SetKeyword(kSearchText);
  data.SetURL("http://abc.com/");
  template_url_service->Add(std::make_unique<TemplateURL>(data));

  // Send "ab", so that an "abc" entry appears in the popup.
  const ui::KeyboardCode kSearchTextPrefixKeys[] = {ui::VKEY_A, ui::VKEY_B,
                                                    ui::VKEY_UNKNOWN};
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextPrefixKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Arrow down to the "abc" entry in the popup.
  size_t size =
      omnibox_view->controller()->autocomplete_controller()->result().size();
  while (omnibox_view->model()->GetPopupSelection().line < size - 1) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, 0));
    if (omnibox_view->GetText() == u"abc")
      break;
  }
  ASSERT_EQ(u"abc", omnibox_view->GetText());

  // Hitting ctrl-enter should navigate based on the current text rather than
  // the original input, i.e. to www.abc.com instead of www.ab.com.
  ASSERT_NO_FATAL_FAILURE(
      NavigateExpectUrl(GURL("http://www.abc.com/"), ui::EF_CONTROL_DOWN));
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, ClearUserTextAfterBackgroundCommit) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Navigate in first tab and enter text into the omnibox.
  GURL url1("data:text/html,page1");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  omnibox_view->SetUserText(u"foo");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create another tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, url1, ui::PAGE_TRANSITION_TYPED));
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
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(ASCIIToUTF16(url2.spec()), omnibox_view->GetText());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, AltEnter) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  omnibox_view->SetUserText(chrome::kChromeUIHistoryURL16);
  int tab_count = browser()->tab_strip_model()->count();
  // alt-Enter opens a new tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, ui::EF_ALT_DOWN));
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count + 1));
}

// TODO(crbug.com/40661918): This test flakily times out.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DISABLED_EnterToSearch) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Test Enter to search.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            omnibox_view->controller()
                ->autocomplete_controller()
                ->result()
                .default_match()
                ->type);
  ASSERT_NO_FATAL_FAILURE(NavigateExpectUrl(GURL(kSearchTextURL)));

  // Test that entering a single character then Enter performs a search.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(omnibox_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, 0));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            omnibox_view->controller()
                ->autocomplete_controller()
                ->result()
                .default_match()
                ->type);
  ASSERT_NO_FATAL_FAILURE(
      NavigateExpectUrl(GURL("http://www.foo.com/search?q=z")));
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, EscapeToDefaultMatch) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  std::u16string old_text = omnibox_view->GetText();

  // Make sure inline autocomplete is triggered.
  EXPECT_GT(old_text.length(), std::size(kInlineAutocompleteText) - 1);

  size_t old_selected_line = omnibox_view->model()->GetPopupSelection().line;
  EXPECT_EQ(0U, old_selected_line);

  // Move to another line with different text.
  size_t size =
      omnibox_view->controller()->autocomplete_controller()->result().size();
  while (omnibox_view->model()->GetPopupSelection().line < size - 1) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, 0));
    ASSERT_NE(old_selected_line,
              omnibox_view->model()->GetPopupSelection().line);
    if (old_text != omnibox_view->GetText()) {
      break;
    }
  }

  EXPECT_NE(old_text, omnibox_view->GetText());

  // Escape shall revert back to the default match item.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_ESCAPE, 0));
  EXPECT_EQ(old_text, omnibox_view->GetText());
  EXPECT_EQ(old_selected_line, omnibox_view->model()->GetPopupSelection().line);
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest,
                       RevertDefaultRevertInlineTextWhenSelectingDefaultMatch) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  std::u16string old_text = omnibox_view->GetText();

  // Make sure inline autocomplete is triggered.
  EXPECT_GT(old_text.length(), std::size(kInlineAutocompleteText) - 1);

  size_t old_selected_line = omnibox_view->model()->GetPopupSelection().line;
  EXPECT_EQ(0U, old_selected_line);

  // Move to another line with different text.
  size_t size =
      omnibox_view->controller()->autocomplete_controller()->result().size();
  while (omnibox_view->model()->GetPopupSelection().line < size - 1) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, 0));
    ASSERT_NE(old_selected_line,
              omnibox_view->model()->GetPopupSelection().line);
    if (old_text != omnibox_view->GetText()) {
      break;
    }
  }

  EXPECT_NE(old_text, omnibox_view->GetText());

  // Move back to the first line
  while (omnibox_view->model()->GetPopupSelection().line > 0) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_UP, 0));
  }

  EXPECT_EQ(old_text, omnibox_view->GetText());
  EXPECT_EQ(old_selected_line, omnibox_view->model()->GetPopupSelection().line);
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, BasicTextOperations) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  chrome::FocusLocationBar(browser());

  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  std::u16string old_text = omnibox_view->GetText();
  EXPECT_EQ(url::kAboutBlankURL16, old_text);
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
#if BUILDFLAG(IS_MAC)
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
  EXPECT_EQ(old_text + u'a', omnibox_view->GetText());

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
#if BUILDFLAG(IS_MAC)
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
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  ASSERT_GT(strlen(url::kAboutBlankURL),
            std::char_traits<char16_t>::length(kSearchKeyword));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Make sure nothing DCHECKs.
  chrome::FocusSearch(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, AcceptKeywordByTypingQuestionMark) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  std::u16string search_keyword(kSearchKeyword);

  // If the user gets into keyword mode by typing '?', they should be put into
  // keyword mode for their default search provider.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_TRUE(omnibox_view->model()->is_keyword_selected());
  ASSERT_EQ(search_keyword, omnibox_view->model()->keyword());
  ASSERT_EQ(std::u16string(), omnibox_view->GetText());

  // If the user press backspace, they should be left with '?' in the omnibox.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
  EXPECT_EQ(u"?", omnibox_view->GetText());
  EXPECT_EQ(std::u16string(), omnibox_view->model()->keyword());
  EXPECT_FALSE(omnibox_view->model()->is_keyword_hint());
  EXPECT_FALSE(omnibox_view->model()->is_keyword_selected());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, SearchDisabledDontCrashOnQuestionMark) {
  policy::PolicyMap policies;
  policies.Set("DefaultSearchProviderEnabled", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
               base::Value(false), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // '?' isn't in the same place on all keyboard layouts, so send the character
  // instead of keystrokes.
  ASSERT_NO_FATAL_FAILURE({
    omnibox_view->OnBeforePossibleChange();
    omnibox_view->SetUserText(u"?");
    omnibox_view->OnAfterPossibleChange(true);
  });
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_FALSE(omnibox_view->model()->is_keyword_selected());
  ASSERT_EQ(u"?", omnibox_view->GetText());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, NonSubstitutingKeywordTest) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  Profile* profile = browser()->profile();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  // Add a non-default substituting keyword.
  TemplateURLData data;
  data.SetShortName(u"Search abc");
  data.SetKeyword(kSearchText);
  data.SetURL("http://abc.com/{searchTerms}");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));

  omnibox_view->SetUserText(std::u16string());

  // Non-default substituting keyword shouldn't be matched by default.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Check if the default match result is Search Primary Provider.
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            omnibox_view->controller()
                ->autocomplete_controller()
                ->result()
                .default_match()
                ->type);
  ASSERT_EQ(kSearchTextURL, omnibox_view->controller()
                                ->autocomplete_controller()
                                ->result()
                                .default_match()
                                ->destination_url.spec());

  omnibox_view->SetUserText(std::u16string());
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_FALSE(omnibox_view->model()->PopupIsOpen());

  // Try a non-substituting keyword.
  template_url_service->Remove(template_url);
  data.SetShortName(u"abc");
  data.SetURL("http://abc.com/");
  template_url_service->Add(std::make_unique<TemplateURL>(data));

  // We always allow exact matches for non-substituting keywords.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());
  ASSERT_EQ(AutocompleteMatchType::HISTORY_KEYWORD,
            omnibox_view->controller()
                ->autocomplete_controller()
                ->result()
                .default_match()
                ->type);
  ASSERT_EQ("http://abc.com/", omnibox_view->controller()
                                   ->autocomplete_controller()
                                   ->result()
                                   .default_match()
                                   ->destination_url.spec());
}

// Flaky. See https://crbug.com/751031.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DISABLED_DeleteItem) {
  // Disable the search provider, to make sure the popup contains only history
  // items.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  model->SetUserSelectedDefaultSearchProvider(nullptr);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  chrome::FocusLocationBar(browser());

  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  std::u16string old_text = omnibox_view->GetText();

  // Input something that can match history items.
  omnibox_view->SetUserText(u"site.com/p");
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Delete the inline autocomplete part.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DELETE, 0));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());
  ASSERT_GE(
      omnibox_view->controller()->autocomplete_controller()->result().size(),
      3U);

  std::u16string user_text = omnibox_view->GetText();
  ASSERT_EQ(u"site.com/p", user_text);
  omnibox_view->SelectAll(true);
  ASSERT_TRUE(omnibox_view->IsSelectAll());

  // Move down.
  size_t default_line = omnibox_view->model()->GetPopupSelection().line;
  omnibox_view->model()->OnUpOrDownPressed(true, false);
  ASSERT_EQ(default_line + 1, omnibox_view->model()->GetPopupSelection().line);
  std::u16string selected_text = omnibox_view->controller()
                                     ->autocomplete_controller()
                                     ->result()
                                     .match_at(default_line + 1)
                                     .fill_into_edit;
  // Temporary text is shown.
  ASSERT_EQ(selected_text, omnibox_view->GetText());
  ASSERT_FALSE(omnibox_view->IsSelectAll());

  // Delete the item.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DELETE, ui::EF_SHIFT_DOWN));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  // The selected line shouldn't be changed, because we have more than two
  // items.
  ASSERT_EQ(default_line + 1, omnibox_view->model()->GetPopupSelection().line);
  // Make sure the item is really deleted.
  ASSERT_NE(selected_text, omnibox_view->controller()
                               ->autocomplete_controller()
                               ->result()
                               .match_at(default_line + 1)
                               .fill_into_edit);
  selected_text = omnibox_view->controller()
                      ->autocomplete_controller()
                      ->result()
                      .match_at(default_line + 1)
                      .fill_into_edit;
  // New temporary text is shown.
  ASSERT_EQ(selected_text, omnibox_view->GetText());

  // Revert to the default match.
  ASSERT_TRUE(omnibox_view->model()->OnEscapeKeyPressed());
  ASSERT_EQ(default_line, omnibox_view->model()->GetPopupSelection().line);
  ASSERT_EQ(user_text, omnibox_view->GetText());
  ASSERT_TRUE(omnibox_view->IsSelectAll());

  // Move down and up to select the default match as temporary text.
  omnibox_view->model()->OnUpOrDownPressed(true, false);
  ASSERT_EQ(default_line + 1, omnibox_view->model()->GetPopupSelection().line);
  omnibox_view->model()->OnUpOrDownPressed(false, false);
  ASSERT_EQ(default_line, omnibox_view->model()->GetPopupSelection().line);

  selected_text = omnibox_view->controller()
                      ->autocomplete_controller()
                      ->result()
                      .match_at(default_line)
                      .fill_into_edit;
  // New temporary text is shown.
  ASSERT_EQ(selected_text, omnibox_view->GetText());
  ASSERT_FALSE(omnibox_view->IsSelectAll());

  // As the current selected item is the new default item, pressing Escape key
  // should revert all directly.
  ASSERT_TRUE(omnibox_view->model()->OnEscapeKeyPressed());
  ASSERT_EQ(old_text, omnibox_view->GetText());
  ASSERT_TRUE(omnibox_view->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, TabAcceptKeyword) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  std::u16string text = kSearchKeyword;

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

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, PersistKeywordModeOnTabSwitch) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());

  // Create a new tab.
  chrome::NewTab(browser());

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  // Make sure we're still in keyword mode.
  ASSERT_TRUE(omnibox_view->model()->is_keyword_selected());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());
  ASSERT_EQ(omnibox_view->GetText(), std::u16string());

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Switch to the second tab and back to the first.
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  // Make sure we're still in keyword mode.
  ASSERT_TRUE(omnibox_view->model()->is_keyword_selected());
  ASSERT_EQ(kSearchKeyword, omnibox_view->model()->keyword());
  ASSERT_EQ(omnibox_view->GetText(), kSearchText);
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest,
                       CtrlKeyPressedWithInlineAutocompleteTest) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  std::u16string old_text = omnibox_view->GetText();

  // Make sure inline autocomplete is triggered.
  EXPECT_GT(old_text.length(), std::size(kInlineAutocompleteText) - 1);

  // Press ctrl key.
  omnibox_view->model()->OnControlKeyChanged(true);

  // Inline autocomplete should still be there.
  EXPECT_EQ(old_text, omnibox_view->GetText());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, UndoRedo) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  chrome::FocusLocationBar(browser());

  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  std::u16string old_text = omnibox_view->GetText();
  EXPECT_EQ(url::kAboutBlankURL16, old_text);
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
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  // Insert text: ﾀﾞ. This is two, 3-byte UTF-8 characters:
  // U+FF80 "HALFWIDTH KATAKANA LETTER TA" and
  // U+FF9E "HALFWIDTH KATAKANA VOICED SOUND MARK".
  omnibox_view->SetUserText(u"\uFF80\uFF9E");
  EXPECT_FALSE(omnibox_view->GetText().empty());

  // Move the cursor to the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, 0));

  // Backspace should delete the character. In http://crbug.com/192743, the bug
  // was that nothing was deleted.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, 0));
#if BUILDFLAG(IS_MAC)
  // Cocoa text fields attach the sound mark and delete the whole thing. This
  // behavior should remain on Mac even when using a toolkit-views browser
  // window.
  EXPECT_TRUE(omnibox_view->GetText().empty());
#else
  // Toolkit-views text fields delete just the sound mark.
  EXPECT_EQ(u"\uFF80", omnibox_view->GetText());
#endif
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, DoesNotUpdateAutocompleteOnBlur) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());
  size_t start, end;
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_TRUE(start != end);
  std::u16string old_autocomplete_text =
      omnibox_view->controller()->autocomplete_controller()->input_.text();

  // Unfocus the omnibox. This should close the popup but should not run
  // autocomplete.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  ASSERT_FALSE(omnibox_view->model()->PopupIsOpen());
  EXPECT_EQ(
      old_autocomplete_text,
      omnibox_view->controller()->autocomplete_controller()->input_.text());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, Paste) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());

  // Paste should yield the expected text and open the popup.
  SetClipboardText(kSearchText);
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_V, kCtrlOrCmdMask));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  EXPECT_EQ(kSearchText, omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Close the popup and select all.
  omnibox_view->CloseOmniboxPopup();
  omnibox_view->SelectAll(false);
  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());

  // Pasting the same text again over itself should re-open the popup.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_V, kCtrlOrCmdMask));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  EXPECT_EQ(kSearchText, omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());
  omnibox_view->CloseOmniboxPopup();
  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());

  // Pasting amid text should yield the expected text and re-open the popup.
  omnibox_view->SetWindowTextAndCaretPos(u"abcd", 2, false, false);
  SetClipboardText(u"123");
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_V, kCtrlOrCmdMask));
  EXPECT_EQ(u"ab123cd", omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Ctrl/Cmd+Alt+V should not paste.
  ASSERT_NO_FATAL_FAILURE(
      SendKey(ui::VKEY_V, kCtrlOrCmdMask | ui::EF_ALT_DOWN));
  EXPECT_EQ(u"ab123cd", omnibox_view->GetText());
  // TODO(msw): Test that AltGr+V does not paste.
}

IN_PROC_BROWSER_TEST_F(OmniboxViewTest, EditSearchEngines) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_EDIT_SEARCH_ENGINES));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  const std::string target_url =
      std::string(chrome::kChromeUISettingsURL) + chrome::kSearchEnginesSubPage;
  EXPECT_EQ(ASCIIToUTF16(target_url), omnibox_view->GetText());
  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());
}

// Flaky test. The below suggestions are in a random order, and the injected
// keys may or may not have registered. Probably https://crbug.com/751031,
// but I believe the whole input mechanism needs to be re-architected.
// What I'd like to see is, after a sequence of keys is injected, we inject
// an artificial input, and, *only* after that input has been registered,
// do we continue.
IN_PROC_BROWSER_TEST_F(OmniboxViewTest,
                       DISABLED_CtrlArrowAfterArrowSuggestions) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Input something to trigger results.
  const ui::KeyboardCode kKeys[] = {ui::VKEY_B, ui::VKEY_A, ui::VKEY_R,
                                    ui::VKEY_UNKNOWN};
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  ASSERT_EQ(u"bar.com/1", omnibox_view->GetText());

  // Arrow down on a suggestion, and omnibox text should be the suggestion.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, 0));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_EQ(u"www.bar.com/2", omnibox_view->GetText());

  // Highlight the last 2 words and the omnibox text should not change.
  // Simulating Ctrl-shift-left only once does not seem to highlight anything
  // on Linux.
#if BUILDFLAG(IS_MAC)
  // Mac uses alt-left/right to select a word.
  const int modifiers = ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN;
#else
  const int modifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN;
#endif
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, modifiers));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, modifiers));
  ASSERT_EQ(u"www.bar.com/2", omnibox_view->GetText());
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

  std::u16string url_a(u"http://www.a.com/");
  std::u16string url_b(u"http://www.b.com/");
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
  SetTestToolbarPermanentText(u"AB");
  // Should be at beginning. Shift+left should do nothing.
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  SetTestToolbarPermanentText(u"CD");
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
  SetTestToolbarPermanentText(u"AB");
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));

  // We reverse select all on Update() so shift-left won't do anything.
  // On Cocoa, shift-left will just anchor it on the left.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(2u, GetSelectionSize(omnibox_view));

  // And shift-right should reduce by one character.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(1u, GetSelectionSize(omnibox_view));
}

class SiteSearchPolicyOmniboxViewTest : public OmniboxViewTest {
 public:
  SiteSearchPolicyOmniboxViewTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{omnibox::kSiteSearchSettingsPolicy,
                              omnibox::kShowFeaturedEnterpriseSiteSearch},
        /*disabled_features=*/{});
  }
  ~SiteSearchPolicyOmniboxViewTest() override = default;

  base::Value CreateSiteSearchPolicyValue(bool featured) {
    base::Value::List policy_value;
    policy_value.Append(
        base::Value::Dict()
            .Set(policy::SiteSearchPolicyHandler::kShortcut,
                 kSiteSearchPolicyKeyword)
            .Set(policy::SiteSearchPolicyHandler::kName, kSiteSearchPolicyName)
            .Set(policy::SiteSearchPolicyHandler::kUrl, kSiteSearchPolicyURL)
            .Set(policy::SiteSearchPolicyHandler::kFeatured, featured));
    return base::Value(std::move(policy_value));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that keyword search works when `SiteSearchSettings` policy is set.
IN_PROC_BROWSER_TEST_F(SiteSearchPolicyOmniboxViewTest, NonFeatured) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               CreateSiteSearchPolicyValue(/*featured=*/false), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Check that new entries have been added to TemplateURLService.
  const TemplateURL* turl =
      TemplateURLServiceFactory::GetForProfile(browser()->profile())
          ->GetTemplateURLForKeyword(kSiteSearchPolicyKeyword);
  ASSERT_TRUE(turl);
  EXPECT_EQ(turl->created_by_policy(),
            TemplateURLData::CreatedByPolicy::kSiteSearch);
  EXPECT_EQ(turl->short_name(), kSiteSearchPolicyName);
  EXPECT_EQ(turl->url(), kSiteSearchPolicyURL);
  EXPECT_FALSE(turl->featured_by_policy());

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSiteSearchPolicyKeywordKeys));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSiteSearchPolicyKeyword, omnibox_view->model()->keyword());

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSiteSearchPolicyKeyword, omnibox_view->model()->keyword());

  // Input something as search text and perform a search.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  EXPECT_EQ(kSiteSearchPolicyTextURL, omnibox_view->controller()
                                          ->autocomplete_controller()
                                          ->result()
                                          .default_match()
                                          ->destination_url.spec());
}

// Verifies that keyword search works when `SiteSearchSettings` policy defines
// a featured search engine.
IN_PROC_BROWSER_TEST_F(SiteSearchPolicyOmniboxViewTest, Featured) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               CreateSiteSearchPolicyValue(/*featured=*/true), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Check that new entries have been added to TemplateURLService.
  const TemplateURL* turl =
      TemplateURLServiceFactory::GetForProfile(browser()->profile())
          ->GetTemplateURLForKeyword(kSiteSearchPolicyKeywordWithAtPrefix);
  ASSERT_TRUE(turl);
  EXPECT_EQ(turl->created_by_policy(),
            TemplateURLData::CreatedByPolicy::kSiteSearch);
  EXPECT_EQ(turl->short_name(), kSiteSearchPolicyName);
  EXPECT_EQ(turl->url(), kSiteSearchPolicyURL);
  EXPECT_TRUE(turl->featured_by_policy());

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_2, ui::EF_SHIFT_DOWN));
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSiteSearchPolicyKeywordKeys));
  ASSERT_TRUE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSiteSearchPolicyKeywordWithAtPrefix,
            omnibox_view->model()->keyword());

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, 0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSiteSearchPolicyKeywordWithAtPrefix,
            omnibox_view->model()->keyword());

  // Input something as search text and perform a search.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  EXPECT_EQ(kSiteSearchPolicyTextURL, omnibox_view->controller()
                                          ->autocomplete_controller()
                                          ->result()
                                          .default_match()
                                          ->destination_url.spec());
}

// Verifies that featured search engine is shown with starter pack on "@" state
// and that the underlying search works.
IN_PROC_BROWSER_TEST_F(SiteSearchPolicyOmniboxViewTest, FeaturedOnArrowDown) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               CreateSiteSearchPolicyValue(/*featured=*/true), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxView(&omnibox_view));

  // Check that new entries have been added to TemplateURLService.
  const TemplateURL* turl =
      TemplateURLServiceFactory::GetForProfile(browser()->profile())
          ->GetTemplateURLForKeyword(kSiteSearchPolicyKeywordWithAtPrefix);
  ASSERT_TRUE(turl);
  EXPECT_EQ(turl->created_by_policy(),
            TemplateURLData::CreatedByPolicy::kSiteSearch);
  EXPECT_EQ(turl->short_name(), kSiteSearchPolicyName);
  EXPECT_EQ(turl->url(), kSiteSearchPolicyURL);
  EXPECT_TRUE(turl->featured_by_policy());

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_2, ui::EF_SHIFT_DOWN));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, /*modifiers=*/0));
  ASSERT_FALSE(omnibox_view->model()->is_keyword_hint());
  ASSERT_EQ(kSiteSearchPolicyKeywordWithAtPrefix,
            omnibox_view->model()->keyword());

  // Input something as search text and perform a search.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  EXPECT_EQ(kSiteSearchPolicyTextURL, omnibox_view->controller()
                                          ->autocomplete_controller()
                                          ->result()
                                          .default_match()
                                          ->destination_url.spec());
}

// Tests for IDN hostnames that contain deviation characters. See
// idn_spoof_checker.h for details.
class NavigationMetricsRecorderIDNABrowserTest : public InProcessBrowserTest {
 public:
  static constexpr char kHistogram[] =
      "Navigation.HostnameHasDeviationCharacters";

  NavigationMetricsRecorderIDNABrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        url::kUseIDNA2008NonTransitional);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  void TypeTextAndNavigate(const std::string& text) {
    OmniboxView* omnibox =
        browser()->window()->GetLocationBar()->GetOmniboxView();

    // Focus the omnibox.
    // If the omnibox already has focus, just notify OmniboxTabHelper.
    if (omnibox->model()->has_focus()) {
      content::WebContents* active_tab =
          browser()->tab_strip_model()->GetActiveWebContents();
      OmniboxTabHelper::FromWebContents(active_tab)
          ->OnFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
    } else {
      browser()->window()->GetLocationBar()->FocusLocation(false);
    }

    // Enter user input mode to prevent spurious unelision.
    omnibox->model()->SetInputInProgress(true);
    omnibox->OnBeforePossibleChange();
    omnibox->SetUserText(base::UTF8ToUTF16(text), true);
    omnibox->OnAfterPossibleChange(true);

    // Press enter and wait for the navigation to finish.
    content::TestNavigationObserver navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), 1);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const Browser* browser) {
              EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
                  browser, ui::VKEY_RETURN, false, false, false, false));
            },
            browser()));
    navigation_observer.Wait();
  }
  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderIDNABrowserTest,
                       IDNA2008Metrics) {
  using UkmEntry = ukm::builders::Navigation_IDNA2008Transition;

  base::HistogramTester histograms;

  auto url_loader_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            network::URLLoaderCompletionStatus status;
            std::string headers =
                "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
            std::string body = "<html>Hello world</html>";
            content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                         params->client.get());
            return true;
          }));

  // Do a search. Shouldn't record metrics.
  TypeTextAndNavigate("faß");
  histograms.ExpectTotalCount(kHistogram, 0);

  // Type a hostname without deviation characters.
  TypeTextAndNavigate("fass.de");
  histograms.ExpectTotalCount(kHistogram, 1);
  histograms.ExpectBucketCount(kHistogram, false, 1);
  histograms.ExpectBucketCount(kHistogram, true, 0);

  EXPECT_TRUE(
      test_ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName).empty());

  // Type a hostname with a deviation character.
  // Do this in a new tab otherwise omnibox will treat the navigation as a
  // reload.
  chrome::NewTab(browser());
  TypeTextAndNavigate("faß.de");
  histograms.ExpectTotalCount(kHistogram, 2);
  histograms.ExpectBucketCount(kHistogram, false, 1);
  histograms.ExpectBucketCount(kHistogram, true, 1);

  // Should have a new UKM entry.
  auto entries = test_ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0],
                                               GURL("https://fass.de"));
  test_ukm_recorder()->ExpectEntryMetric(
      entries[0], "Character",
      static_cast<int>(IDNA2008DeviationCharacter::kEszett));

  // Should also work with full URLs.
  TypeTextAndNavigate("https://faß.de/test_url");
  histograms.ExpectTotalCount(kHistogram, 3);
  histograms.ExpectBucketCount(kHistogram, false, 1);
  histograms.ExpectBucketCount(kHistogram, true, 2);

  // Should have a new UKM entry.
  entries = test_ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(2u, entries.size());
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0],
                                               GURL("https://fass.de"));
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[1],
                                               GURL("https://faß.de/test_url"));
  test_ukm_recorder()->ExpectEntryMetric(
      entries[0], "Character",
      static_cast<int>(IDNA2008DeviationCharacter::kEszett));
  test_ukm_recorder()->ExpectEntryMetric(
      entries[1], "Character",
      static_cast<int>(IDNA2008DeviationCharacter::kEszett));

  // Reload. Shouldn't record additional metrics since we only care about first
  // time navigations.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  histograms.ExpectTotalCount(kHistogram, 3);
  histograms.ExpectBucketCount(kHistogram, false, 1);
  histograms.ExpectBucketCount(kHistogram, true, 2);

  // Shouldn't record deviation characters outside the hostname.
  TypeTextAndNavigate("https://example.com/faß");
  histograms.ExpectTotalCount(kHistogram, 4);
  histograms.ExpectBucketCount(kHistogram, false, 2);
  histograms.ExpectBucketCount(kHistogram, true, 2);

  // Shouldn't record metrics for non-HTTP/HTTPS.
  TypeTextAndNavigate("data:faß.de");
  histograms.ExpectTotalCount(kHistogram, 4);
  histograms.ExpectBucketCount(kHistogram, false, 2);
  histograms.ExpectBucketCount(kHistogram, true, 2);

  // Shouldn't have any new UKM entries.
  entries = test_ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(2u, entries.size());
}
