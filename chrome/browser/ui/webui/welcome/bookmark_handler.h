// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class PrefService;

namespace welcome {

class BookmarkHandler : public content::WebUIMessageHandler {
 public:
  explicit BookmarkHandler(PrefService* prefs);

  BookmarkHandler(const BookmarkHandler&) = delete;
  BookmarkHandler& operator=(const BookmarkHandler&) = delete;

  ~BookmarkHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Callbacks for JS APIs.
  void HandleToggleBookmarkBar(const base::Value::List& args);
  void HandleIsBookmarkBarShown(const base::Value::List& args);

 private:
  // Weak reference.
  raw_ptr<PrefService> prefs_;
};

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_HANDLER_H_
