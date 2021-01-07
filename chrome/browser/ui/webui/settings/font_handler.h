// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace base {
class ListValue;
}  // namespace base

namespace settings {

// Handle OS font list and font preference settings.
class FontHandler : public SettingsPageUIHandler {
 public:
  explicit FontHandler(Profile* profile);
  ~FontHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Handler for script asking for font information.
  void HandleFetchFontsData(const base::ListValue* args);

  // Callback to handle fonts loading.
  void FontListHasLoaded(std::string callback_id,
                         std::unique_ptr<base::ListValue> list);

  base::WeakPtrFactory<FontHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FontHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_
