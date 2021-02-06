// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class PrefService;

namespace welcome {

class BookmarkHandler : public content::WebUIMessageHandler {
 public:
  explicit BookmarkHandler(PrefService* prefs);
  ~BookmarkHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Callbacks for JS APIs.
  void HandleToggleBookmarkBar(const base::ListValue* args);
  void HandleIsBookmarkBarShown(const base::ListValue* args);

 private:
  // Weak reference.
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkHandler);
};

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_HANDLER_H_
