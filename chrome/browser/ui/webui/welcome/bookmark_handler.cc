// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/bookmark_handler.h"

#include "base/bind.h"
#include "chrome/grit/browser_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"

namespace welcome {

BookmarkHandler::BookmarkHandler(PrefService* prefs) : prefs_(prefs) {}

BookmarkHandler::~BookmarkHandler() {}

void BookmarkHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "toggleBookmarkBar",
      base::BindRepeating(&BookmarkHandler::HandleToggleBookmarkBar,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "isBookmarkBarShown",
      base::BindRepeating(&BookmarkHandler::HandleIsBookmarkBarShown,
                          base::Unretained(this)));
}

void BookmarkHandler::HandleToggleBookmarkBar(const base::ListValue* args) {
  const auto& list = args->GetListDeprecated();
  CHECK(!list.empty());
  const bool show = list[0].GetBool();
  prefs_->SetBoolean(bookmarks::prefs::kShowBookmarkBar, show);
}

void BookmarkHandler::HandleIsBookmarkBarShown(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetListDeprecated().size());
  const base::Value& callback_id = args->GetListDeprecated()[0];

  ResolveJavascriptCallback(
      callback_id,
      base::Value(prefs_->GetBoolean(bookmarks::prefs::kShowBookmarkBar)));
}

}  // namespace welcome
