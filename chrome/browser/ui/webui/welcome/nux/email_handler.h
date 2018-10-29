// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace nux {

extern const char* kEmailInteractionHistogram;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EmailInteraction {
  kPromptShown = 0,
  kNoThanks = 1,
  kGetStarted = 2,
  kCount,
};

class EmailHandler : public content::WebUIMessageHandler {
 public:
  explicit EmailHandler(favicon::FaviconService* favicon_service);
  ~EmailHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Callbacks for JS APIs.
  void HandleCacheEmailIcon(const base::ListValue* args);
  void HandleGetEmailList(const base::ListValue* args);

  // Adds webui sources.
  static void AddSources(content::WebUIDataSource* html_source);

 private:
  // Weak reference.
  favicon::FaviconService* favicon_service_;

  DISALLOW_COPY_AND_ASSIGN(EmailHandler);
};

}  // namespace nux

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_HANDLER_H_
