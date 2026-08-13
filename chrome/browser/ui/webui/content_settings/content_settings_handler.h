// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTENT_SETTINGS_CONTENT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONTENT_SETTINGS_CONTENT_SETTINGS_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/content_settings/content_settings_internals.mojom.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content_settings_internals {

// Mojo handler for chrome://content-settings WebUI page.
class ContentSettingsHandler
    : public content_settings_internals::mojom::PageHandler {
 public:
  explicit ContentSettingsHandler(
      Profile* profile,
      mojo::PendingReceiver<content_settings_internals::mojom::PageHandler>
          pending_receiver);

  ~ContentSettingsHandler() override;

  ContentSettingsHandler(ContentSettingsHandler&&) = delete;
  ContentSettingsHandler(const ContentSettingsHandler&) = delete;
  ContentSettingsHandler& operator=(const ContentSettingsHandler&) = delete;

  void ReadContentSettings(const ContentSettingsType type,
                           ReadContentSettingsCallback callback) override;

  void ContentSettingsPatternToString(
      const ContentSettingsPattern& pattern,
      ContentSettingsPatternToStringCallback callback) override;

  void StringToContentSettingsPattern(
      const std::string& s,
      StringToContentSettingsPatternCallback callback) override;

 private:
  raw_ptr<Profile> profile_ = nullptr;
  mojo::Receiver<content_settings_internals::mojom::PageHandler> receiver_{
      this};
};

}  // namespace content_settings_internals

#endif  // CHROME_BROWSER_UI_WEBUI_CONTENT_SETTINGS_CONTENT_SETTINGS_HANDLER_H_
