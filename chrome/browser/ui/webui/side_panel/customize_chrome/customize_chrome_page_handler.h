// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

class CustomizeChromePageHandler
    : public side_panel::mojom::CustomizeChromePageHandler {
 public:
  CustomizeChromePageHandler(
      mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
          receiver,
      Profile* profile);

  CustomizeChromePageHandler(const CustomizeChromePageHandler&) = delete;
  CustomizeChromePageHandler& operator=(const CustomizeChromePageHandler&) =
      delete;

  ~CustomizeChromePageHandler() override;

  // side_panel::mojom::CustomizeChromePageHandler:
  void SetMostVisitedSettings(bool custom_links_enabled, bool visible) override;
  void GetMostVisitedSettings(GetMostVisitedSettingsCallback callback) override;

 private:
  bool IsCustomLinksEnabled() const;
  bool IsShortcutsVisible() const;

  mojo::Receiver<side_panel::mojom::CustomizeChromePageHandler> receiver_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
