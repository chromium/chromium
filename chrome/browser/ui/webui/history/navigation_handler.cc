// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/navigation_handler.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"

namespace webui {

NavigationHandler::NavigationHandler() {}

NavigationHandler::~NavigationHandler() {}

void NavigationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "navigateToUrl",
      base::BindRepeating(&NavigationHandler::HandleNavigateToUrl,
                          base::Unretained(this)));
}

void NavigationHandler::HandleNavigateToUrl(const base::Value::List& list) {
  const std::string& url_string = list[0].GetString();
  const std::string& target_string = list[1].GetString();
  double button = list[2].GetDouble();
  bool alt_key = list[3].GetBool();
  bool ctrl_key = list[4].GetBool();
  bool meta_key = list[5].GetBool();
  bool shift_key = list[6].GetBool();

  CHECK(button == 0.0 || button == 1.0);
  bool middle_button = (button == 1.0);

  WindowOpenDisposition disposition = ui::DispositionFromClick(
      middle_button, alt_key, ctrl_key, meta_key, shift_key,
      (target_string == "_blank") ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                  : WindowOpenDisposition::CURRENT_TAB);
  web_ui()->GetWebContents()->OpenURL(
      content::OpenURLParams(GURL(url_string), content::Referrer(), disposition,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});

  // This may delete us!
}

}  // namespace webui
