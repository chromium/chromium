// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/bookmark_handler.h"

#include "base/bind.h"
#include "chrome/grit/browser_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"

namespace nux {

BookmarkHandler::BookmarkHandler(PrefService* prefs) : prefs_(prefs) {}

BookmarkHandler::~BookmarkHandler() {}

void BookmarkHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "toggleBookmarkBar",
      base::BindRepeating(&BookmarkHandler::HandleToggleBookmarkBar,
                          base::Unretained(this)));
}

void BookmarkHandler::HandleToggleBookmarkBar(const base::ListValue* args) {
  bool show;
  CHECK(args->GetBoolean(0, &show));
  prefs_->SetBoolean(bookmarks::prefs::kShowBookmarkBar, show);
}

void BookmarkHandler::AddSources(content::WebUIDataSource* html_source,
                                 PrefService* prefs) {
  // Add constants to loadtime data
  html_source->AddBoolean(
      "bookmark_bar_shown",
      prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  html_source->SetJsonPath("strings.js");
}

}  // namespace nux
