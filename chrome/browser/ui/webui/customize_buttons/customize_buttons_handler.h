// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CUSTOMIZE_BUTTONS_CUSTOMIZE_BUTTONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CUSTOMIZE_BUTTONS_CUSTOMIZE_BUTTONS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

class CustomizeButtonsHandler
    : public customize_buttons::mojom::CustomizeButtonsHandler {
 public:
  CustomizeButtonsHandler(
      mojo::PendingReceiver<customize_buttons::mojom::CustomizeButtonsHandler>
          pending_handler,
      mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
          pending_page,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<NewTabPageFeaturePromoHelper>
          customize_chrome_feature_promo_helper);
  CustomizeButtonsHandler(const CustomizeButtonsHandler&) = delete;
  CustomizeButtonsHandler& operator=(const CustomizeButtonsHandler&) = delete;
  ~CustomizeButtonsHandler() override;

  // Called when the embedding TabInterface has changed.
  // TODO(crbug.com/378475391): This can be removed once the NTP has been
  // restricted from loading in app windows.
  void OnTabInterfaceChanged();

  void NotifyCustomizeChromeSidePanelVisibilityChanged(bool is_open);

  // customize_buttons::mojom::CustomizeButtonsHandler:
  void IncrementCustomizeChromeButtonOpenCount() override;
  void IncrementWallpaperSearchButtonShownCount() override;
  void SetCustomizeChromeSidePanelVisible(
      bool visible,
      customize_buttons::mojom::CustomizeChromeSection section,
      customize_buttons::mojom::SidePanelOpenTrigger triger) override;

  void SetCustomizeChromeSidePanelControllerForTesting(
      customize_chrome::SidePanelController* side_panel_controller);

 private:
  void SetCustomizeChromeSidePanelController(
      customize_chrome::SidePanelController* side_panel_controller);

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NewTabPageFeaturePromoHelper> feature_promo_helper_;

  // TODO(crbug.com/378475391): Make this const once the TabModel is guaranteed
  // to be present during load and fixed for the NTP's lifetime.
  raw_ptr<customize_chrome::SidePanelController>
      customize_chrome_side_panel_controller_;
  base::CallbackListSubscription tab_changed_subscription_;

  mojo::Remote<customize_buttons::mojom::CustomizeButtonsDocument> page_;
  mojo::Receiver<customize_buttons::mojom::CustomizeButtonsHandler> receiver_;

  base::WeakPtrFactory<CustomizeButtonsHandler> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_UI_WEBUI_CUSTOMIZE_BUTTONS_CUSTOMIZE_BUTTONS_HANDLER_H_
