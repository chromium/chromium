// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"

#include <memory>

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_commerce_resources.h"
#include "chrome/grit/side_panel_commerce_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/mojom/shopping_list.mojom.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

ShoppingInsightsSidePanelUI::ShoppingInsightsSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, commerce::kChromeUIShoppingInsightsSidePanelHost);

  webui::SetupWebUIDataSource(source,
                              base::make_span(kSidePanelCommerceResources,
                                              kSidePanelCommerceResourcesSize),
                              IDR_SIDE_PANEL_COMMERCE_SHOPPING_INSIGHTS_HTML);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));
}

ShoppingInsightsSidePanelUI::~ShoppingInsightsSidePanelUI() = default;

void ShoppingInsightsSidePanelUI::BindInterface(
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandlerFactory>
        receiver) {
  shopping_list_factory_receiver_.reset();
  shopping_list_factory_receiver_.Bind(std::move(receiver));
}

void ShoppingInsightsSidePanelUI::CreateShoppingListHandler(
    mojo::PendingRemote<shopping_list::mojom::Page> page,
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  shopping_list_handler_ = std::make_unique<commerce::ShoppingListHandler>(
      std::move(page), std::move(receiver), bookmark_model, shopping_service,
      profile->GetPrefs(), tracker, g_browser_process->GetApplicationLocale(),
      std::make_unique<commerce::ShoppingUiHandlerDelegate>());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ShoppingInsightsSidePanelUI)
