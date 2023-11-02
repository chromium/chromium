// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_

#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

class BookmarksMessageHandler : public content::WebUIMessageHandler {
 public:
  BookmarksMessageHandler();

  BookmarksMessageHandler(const BookmarksMessageHandler&) = delete;
  BookmarksMessageHandler& operator=(const BookmarksMessageHandler&) = delete;

  ~BookmarksMessageHandler() override;

 private:
  int GetIncognitoAvailability();
  void HandleGetIncognitoAvailability(const base::Value::List& args);
  void UpdateIncognitoAvailability();

  bool CanEditBookmarks();
  void HandleGetCanEditBookmarks(const base::Value::List& args);
  void UpdateCanEditBookmarks();

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_
