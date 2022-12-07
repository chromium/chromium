// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

class CustomizeChromePageHandler
    : public side_panel::mojom::CustomizeChromePageHandler,
      public NtpBackgroundServiceObserver {
 public:
  CustomizeChromePageHandler(
      mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
          pending_page_handler,
      mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
      NtpCustomBackgroundService* ntp_custom_background_service,
      content::WebContents* web_contents);

  CustomizeChromePageHandler(const CustomizeChromePageHandler&) = delete;
  CustomizeChromePageHandler& operator=(const CustomizeChromePageHandler&) =
      delete;

  ~CustomizeChromePageHandler() override;

  // side_panel::mojom::CustomizeChromePageHandler:
  void SetMostVisitedSettings(bool custom_links_enabled, bool visible) override;
  void GetMostVisitedSettings(GetMostVisitedSettingsCallback callback) override;
  void GetChromeColors(GetChromeColorsCallback callback) override;
  void GetBackgroundCollections(
      GetBackgroundCollectionsCallback callback) override;
  void UpdateTheme() override;

 private:
  bool IsCustomLinksEnabled() const;
  bool IsShortcutsVisible() const;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<NtpBackgroundService> ntp_background_service_;
  GetBackgroundCollectionsCallback background_collections_callback_;
  base::TimeTicks background_collections_request_start_time_;

  mojo::Remote<side_panel::mojom::CustomizeChromePage> page_;
  mojo::Receiver<side_panel::mojom::CustomizeChromePageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
