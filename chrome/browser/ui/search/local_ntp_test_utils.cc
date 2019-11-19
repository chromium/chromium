// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/local_ntp_test_utils.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/resource/resource_bundle.h"
namespace local_ntp_test_utils {

content::WebContents* OpenNewTab(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  return browser->tab_strip_model()->GetActiveWebContents();
}

void NavigateToNTPAndWaitUntilLoaded(Browser* browser, int delay) {
  content::WebContents* active_tab =
      browser->tab_strip_model()->GetActiveWebContents();

  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a message queue *before* navigating to the NTP, to make sure we
  // don't miss the 'loaded' message due to some race condition.
  content::DOMMessageQueue msg_queue(active_tab);

  // Navigate to the NTP.
  ui_test_utils::NavigateToURL(browser, GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  WaitUntilTilesLoaded(active_tab, &msg_queue, delay);
}

void ExecuteScriptOnNTPAndWaitUntilLoaded(content::RenderFrameHost* host,
                                          const std::string& script) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(host);

  ASSERT_TRUE(search::IsInstantNTP(contents));

  // Attach a message queue *before* executing the script, to make sure we don't
  // miss the 'loaded' message due to some race condition.
  content::DOMMessageQueue msg_queue(contents);

  // Execute the script, and get rid of the message that the ExecuteScript call
  // produces.
  EXPECT_TRUE(content::ExecuteScript(host, script));
  std::string message;
  ASSERT_TRUE(msg_queue.PopMessage(&message));

  WaitUntilTilesLoaded(contents, &msg_queue, /*delay=*/0);
}

void WaitUntilTilesLoaded(content::WebContents* active_tab,
                          content::DOMMessageQueue* msg_queue,
                          int delay) {
  // At this point, the MV iframe may or may not have been fully loaded. Once
  // it loads, it sends a 'loaded' postMessage to the page. Check if the page
  // has already received that, and if not start listening for it. It's
  // important that these two things happen in the same JS invocation, since
  // otherwise we might miss the message.
  bool mv_tiles_loaded = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, base::StringPrintf(R"js(
      (function() {
        if (tilesAreLoaded) {
          tilesAreLoaded = false;
          return true;
        }
        const loaded = (event) => {
          if (event.data.cmd == 'loaded') {
            window.removeEventListener('message', loaded);
            setTimeout(() => {
              tilesAreLoaded = false;
              domAutomationController.send('WaitUntilTilesLoaded');
            }, %d);
          }
        };
        window.addEventListener('message', loaded);
        return false;
      })()
      )js", delay), &mv_tiles_loaded));

  std::string message;
  // Get rid of the message that the GetBoolFromJS call produces.
  ASSERT_TRUE(msg_queue->PopMessage(&message));

  if (mv_tiles_loaded) {
    // The tiles are already loaded, i.e. we missed the 'loaded' message. All
    // is well.
    return;
  }

  // Not loaded yet. Wait for the "WaitUntilTilesLoaded" message.
  ASSERT_TRUE(msg_queue->WaitForMessage(&message));
  ASSERT_EQ("\"WaitUntilTilesLoaded\"", message);
  // There shouldn't be any other messages.
  ASSERT_FALSE(msg_queue->PopMessage(&message));
}

bool SwitchBrowserLanguageToFrench() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Make sure the default language is not French.
  std::string default_locale = g_browser_process->GetApplicationLocale();
  EXPECT_NE("fr", default_locale);

  // Switch browser language to French.
  g_browser_process->SetApplicationLocale("fr");
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(language::prefs::kApplicationLocale, "fr");

  std::string loaded_locale =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("fr");

  return loaded_locale == "fr";
}

void SetUserSelectedDefaultSearchProvider(Profile* profile,
                                          const std::string& base_url,
                                          const std::string& ntp_url) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(base_url));
  data.SetKeyword(base::UTF8ToUTF16(base_url));
  data.SetURL(base_url + "url?bar={searchTerms}");
  data.new_tab_url = ntp_url;

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
}

GURL GetFinalNtpUrl(Profile* profile) {
  if (search::GetNewTabPageURL(profile) == chrome::kChromeSearchLocalNtpUrl) {
    // If chrome://newtab/ already maps to the local NTP, then that will load
    // correctly, even without network.  The URL associated with the WebContents
    // will stay chrome://newtab/
    return GURL(chrome::kChromeUINewTabURL);
  }
  // If chrome://newtab/ maps to a remote URL, then it will fail to load in a
  // browser_test environment.  In this case, we will get redirected to the
  // local NTP, which changes the URL associated with the WebContents.
  return GURL(chrome::kChromeSearchLocalNtpUrl);
}

}  // namespace local_ntp_test_utils
