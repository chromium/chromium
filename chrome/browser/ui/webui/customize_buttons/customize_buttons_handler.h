// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CUSTOMIZE_BUTTONS_CUSTOMIZE_BUTTONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CUSTOMIZE_BUTTONS_CUSTOMIZE_BUTTONS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "components/tabs/public/tab_interface.h"
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
      content::WebUI* web_ui,
      // `CustomizeButtonsHandler` can be embedded either in a tab or a
      // BrowserView. If embedded in a tab, the caller should pass the
      // corresponding `TabInterface`. Otherwise, the handler will fall back to
      // the active tab's `TabInterface` if available.
      tabs::TabInterface* tab_interface,
      std::unique_ptr<NewTabPageFeaturePromoHelper>
          customize_chrome_feature_promo_helper);
  CustomizeButtonsHandler(const CustomizeButtonsHandler&) = delete;
  CustomizeButtonsHandler& operator=(const CustomizeButtonsHandler&) = delete;
  ~CustomizeButtonsHandler() override;

  // customize_buttons::mojom::CustomizeButtonsHandler:
  void IncrementCustomizeChromeButtonOpenCount() override;
  void IncrementWallpaperSearchButtonShownCount() override;
  // TODO(crbug.com/419368727) Remove |visible| as part of deprecating the
  // Wallpaper Search button.
  void SetCustomizeChromeSidePanelVisible(
      bool visible,
      customize_buttons::mojom::CustomizeChromeSection section,
      customize_buttons::mojom::SidePanelOpenTrigger triger) override;

 private:
  tabs::TabInterface* GetActiveTab();
  customize_chrome::SidePanelController* GetSidePanelControllerForActiveTab();
  // TODO(crbug.com/419368727) Remove
  // NotifyCustomizeChromeSidePanelVisibilityChanged() as part of deprecating
  // the Wallpaper Search button.
  void NotifyCustomizeChromeSidePanelVisibilityChanged(bool is_open);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void SetCustomizeChromeEntryChangedCallback(tabs::TabInterface* tab);

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  raw_ptr<Profile> profile_;
  const raw_ptr<content::WebUI> web_ui_;
  raw_ptr<tabs::TabInterface> tab_interface_;
  std::unique_ptr<NewTabPageFeaturePromoHelper> feature_promo_helper_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<customize_buttons::mojom::CustomizeButtonsDocument> page_;
  mojo::Receiver<customize_buttons::mojom::CustomizeButtonsHandler> receiver_;

  base::WeakPtrFactory<CustomizeButtonsHandler> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_UI_WEBUI_CUSTOMIZE_BUTTONS_CUSTOMIZE_BUTTONS_HANDLER_H_
