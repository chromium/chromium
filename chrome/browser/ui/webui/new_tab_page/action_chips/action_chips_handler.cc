// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/mojom/url.mojom.h"

ActionChipsHandler::ActionChipsHandler(
    mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> receiver,
    Profile* profile,
    content::WebUI* web_ui)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      web_ui_(web_ui) {}

ActionChipsHandler::~ActionChipsHandler() = default;

void ActionChipsHandler::GetMostRecentTab(GetMostRecentTabCallback callback) {
  content::WebContents* contents = FindMostRecentTab();
  if (!contents) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto tab_info = action_chips::mojom::TabInfo::New();
  tab_info->tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
  tab_info->title = base::UTF16ToUTF8(contents->GetTitle());
  tab_info->url = contents->GetLastCommittedURL();
  tab_info->last_active_time = contents->GetLastActiveTime();
  std::move(callback).Run(std::move(tab_info));
}

content::WebContents* ActionChipsHandler::FindMostRecentTab() {
  content::WebContents* web_contents = web_ui_->GetWebContents();
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser_window_interface) {
    return nullptr;
  }

  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  content::WebContents* most_recent_contents = nullptr;
  base::Time latest_active_time;

  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    if (contents == web_contents) {
      continue;
    }
    if (contents->GetLastCommittedURL().SchemeIs(content::kChromeUIScheme)) {
      continue;
    }
    if (contents->GetLastCommittedURL().IsAboutBlank()) {
      continue;
    }
    base::Time last_active = contents->GetLastActiveTime();
    if (last_active > latest_active_time) {
      latest_active_time = last_active;
      most_recent_contents = contents;
    }
  }

  return most_recent_contents;
}
