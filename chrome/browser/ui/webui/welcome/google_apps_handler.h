// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_GOOGLE_APPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_GOOGLE_APPS_HANDLER_H_

#include <vector>

#include "base/values.h"
#include "chrome/browser/ui/webui/welcome/bookmark_item.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace welcome {

extern const char* kGoogleAppsInteractionHistogram;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GoogleAppsInteraction {
  kPromptShown = 0,
  kNoThanks = 1,
  kGetStarted = 2,
  kCount,
};

class GoogleAppsHandler : public content::WebUIMessageHandler {
 public:
  GoogleAppsHandler();

  GoogleAppsHandler(const GoogleAppsHandler&) = delete;
  GoogleAppsHandler& operator=(const GoogleAppsHandler&) = delete;

  ~GoogleAppsHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Callbacks for JS APIs.
  void HandleCacheGoogleAppIcon(const base::Value::List& args);
  void HandleGetGoogleAppsList(const base::Value::List& args);

 private:
  std::vector<BookmarkItem> google_apps_;
};

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_GOOGLE_APPS_HANDLER_H_
