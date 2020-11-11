// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/navigation_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/window_open_disposition.h"

namespace webui {

NavigationHandler::NavigationHandler() {}

NavigationHandler::~NavigationHandler() {}

void NavigationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "navigateToUrl",
      base::BindRepeating(&NavigationHandler::HandleNavigateToUrl,
                          base::Unretained(this)));
}

void NavigationHandler::HandleNavigateToUrl(const base::ListValue* args) {
  std::string url_string;
  std::string target_string;
  double button;
  bool alt_key;
  bool ctrl_key;
  bool meta_key;
  bool shift_key;

  CHECK(args->GetString(0, &url_string));
  CHECK(args->GetString(1, &target_string));
  CHECK(args->GetDouble(2, &button));
  CHECK(args->GetBoolean(3, &alt_key));
  CHECK(args->GetBoolean(4, &ctrl_key));
  CHECK(args->GetBoolean(5, &meta_key));
  CHECK(args->GetBoolean(6, &shift_key));

  CHECK(button == 0.0 || button == 1.0);
  bool middle_button = (button == 1.0);

  WindowOpenDisposition disposition = ui::DispositionFromClick(
      middle_button, alt_key, ctrl_key, meta_key, shift_key,
      (target_string == "_blank") ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                  : WindowOpenDisposition::CURRENT_TAB);
  web_ui()->GetWebContents()->OpenURL(
      content::OpenURLParams(GURL(url_string), content::Referrer(), disposition,
                             ui::PAGE_TRANSITION_LINK, false));

  // This may delete us!
}

}  // namespace webui
