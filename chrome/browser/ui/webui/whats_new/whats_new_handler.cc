// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

WhatsNewHandler::WhatsNewHandler() = default;

WhatsNewHandler::~WhatsNewHandler() = default;

void WhatsNewHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "initialize", base::BindRepeating(&WhatsNewHandler::HandleInitialize,
                                        base::Unretained(this)));
}

void WhatsNewHandler::OnJavascriptAllowed() {}

void WhatsNewHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WhatsNewHandler::HandleInitialize(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  bool is_auto;
  CHECK(args->GetBoolean(1, &is_auto));

  AllowJavascript();
  if (whats_new::IsRemoteContentDisabled()) {
    // Just resolve with failure. This shows the error page which is all local
    // content, so that we don't trigger potentially flaky network requests in
    // tests.
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  std::string url = whats_new::GetURLForVersion(CHROME_VERSION_MAJOR);
  // Send back the initial URL for preload, to reduce delay loading the page.
  FireWebUIListener("preload-url", base::Value(url));
  fetcher_ = std::make_unique<whats_new::WhatsNewFetcher>(
      CHROME_VERSION_MAJOR, is_auto,
      base::BindOnce(&WhatsNewHandler::OnFetchResult,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void WhatsNewHandler::OnFetchResult(const std::string& callback_id,
                                    bool is_auto,
                                    bool success,
                                    bool page_not_found,
                                    std::unique_ptr<std::string> body) {
  if (!success && is_auto) {
    Browser* browser = chrome::FindLastActive();
    if (!browser)
      return;

    if (browser->tab_strip_model()->count() == 1) {
      // Don't close the tab if this is the only tab as doing so will close the
      // browser right after the user tried to start it. Instead, load the NTP
      // as a fallback startup experience if What's New failed to load when
      // being shown automatically.
      whats_new::LogLoadEvent(whats_new::LoadEvent::kLoadFailAndFallbackToNtp);
      content::OpenURLParams params(GURL(chrome::kChromeUINewTabPageURL),
                                    content::Referrer(),
                                    WindowOpenDisposition::CURRENT_TAB,
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
      browser->OpenURL(params);
    } else {
      // If other startup tabs already exist, close the tab in order to show
      // them. This destroys the handler so don't do anything else after this.
      whats_new::LogLoadEvent(whats_new::LoadEvent::kLoadFailAndCloseTab);
      content::WebContents* contents = web_ui()->GetWebContents();
      chrome::CloseWebContents(browser, contents, /* add_to_history= */ false);
    }
  } else {
    whats_new::LogLoadEvent(success
                                ? whats_new::LoadEvent::kLoadSuccess
                                : whats_new::LoadEvent::kLoadFailAndShowError);
    ResolveJavascriptCallback(
        base::Value(callback_id),
        success ? base::Value(whats_new::GetURLForVersion(CHROME_VERSION_MAJOR))
                : base::Value());
  }
}
