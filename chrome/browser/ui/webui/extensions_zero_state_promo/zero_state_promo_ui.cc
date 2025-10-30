// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_ui.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/extensions_zero_state_promo_resources.h"
#include "chrome/grit/extensions_zero_state_promo_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"
#include "ui/webui/webui_util.h"

namespace extensions {

DEFINE_TOP_CHROME_WEBUI_CONFIG(ZeroStatePromoController)

ZeroStatePromoController::ZeroStatePromoController(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui, true) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIExtensionsZeroStatePromoHost);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"extensionsZeroStateIphHeader", IDS_EXTENSIONS_ZERO_STATE_IPH_HEADER},
      {"extensionsZeroStateIphHeaderV2",
       IDS_EXTENSIONS_ZERO_STATE_IPH_HEADER_V2},
      {"extensionsZeroStateChipsIphDesc",
       IDS_EXTENSIONS_ZERO_STATE_CHIPS_IPH_DESCRIPTION},
      {"extensionsZeroStateChipsWithLinkIphLinkLabel",
       IDS_EXTENSIONS_ZERO_STATE_CHIPS_WITH_LINK_IPH_LINK_LABEL},
      {"extensionsZeroStateChipsWithLinkIphDesc",
       IDS_EXTENSIONS_ZERO_STATE_CHIPS_WITH_LINK_IPH_DESCRIPTION},
      {"extensionsZeroStatePlainLinkIphDesc",
       IDS_EXTENSIONS_ZERO_STATE_PLAIN_LINK_IPH_DESCRIPTION},
      {"extensionsZeroStateIphShoppingCategoryLabel",
       IDS_EXTENSIONS_ZERO_STATE_IPH_SHOPPING_CATEGORY_LABEL},
      {"extensionsZeroStateIphWritingHelpCollectionLabel",
       IDS_EXTENSIONS_ZERO_STATE_IPH_WRITING_HELP_COLLECTION_LABEL},
      {"extensionsZeroStateIphProductivityCategoryLabel",
       IDS_EXTENSIONS_ZERO_STATE_IPH_PRODUCTIVITY_CATEGORY_LABEL},
      {"extensionsZeroStateIphAiProductivityCollectionLabel",
       IDS_EXTENSIONS_ZERO_STATE_IPH_AI_PRODUCTIVITY_COLLECTION_LABEL},
      {"extensionsZeroStateIphDismissButtonTitle",
       IDS_EXTENSIONS_ZERO_STATE_IPH_DISMISS_BUTTON_TITLE},
      {"extensionsZeroStateIphShoppingCategoryLink",
       IDS_EXTENSIONS_ZERO_STATE_IPH_SHOPPING_CATEGORY_LINK},
      {"extensionsZeroStateIphWritingHelpCollectionLink",
       IDS_EXTENSIONS_ZERO_STATE_IPH_WRITING_HELP_COLLECTION_LINK},
      {"extensionsZeroStateIphProductivityCategoryLink",
       IDS_EXTENSIONS_ZERO_STATE_IPH_PRODUCTIVITY_CATEGORY_LINK},
      {"extensionsZeroStateIphAiProductivityCollectionLink",
       IDS_EXTENSIONS_ZERO_STATE_IPH_AI_PRODUCTIVITY_COLLECTION_LINK},
      {"extensionsZeroStateIphWebStoreLink",
       IDS_EXTENSIONS_ZERO_STATE_IPH_WEB_STORE_LINK},
      {"extensionsZeroStateIphCloseButtonLabel",
       IDS_EXTENSIONS_ZERO_STATE_IPH_DISMISS_BUTTON_TEXT},
      {"extensionsZeroStateIphCustomActionButtonLabel",
       IDS_EXTENSIONS_ZERO_STATE_PROMO_CUSTOM_ACTION_IPH_ACCEPT},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  feature_engagement::IPHExtensionsZeroStatePromoVariant promoVariant =
      feature_engagement::kIPHExtensionsZeroStatePromoVariantParam.Get();
  source->AddBoolean("showPlainLinksUi",
                     feature_engagement::IPHExtensionsZeroStatePromoVariant::
                             kCustomUIPlainLinkIph == promoVariant);
  source->AddBoolean("showChipsUiV1",
                     feature_engagement::IPHExtensionsZeroStatePromoVariant::
                             kCustomUiChipIphV1 == promoVariant);
  source->AddBoolean("showChipsUiV2",
                     feature_engagement::IPHExtensionsZeroStatePromoVariant::
                             kCustomUiChipIphV2 == promoVariant);
  source->AddBoolean("showChipsUiV3",
                     feature_engagement::IPHExtensionsZeroStatePromoVariant::
                             kCustomUiChipIphV3 == promoVariant);

  webui::SetupWebUIDataSource(
      source, kExtensionsZeroStatePromoResources,
      IDR_EXTENSIONS_ZERO_STATE_PROMO_ZERO_STATE_PROMO_HTML);
}

ZeroStatePromoController::~ZeroStatePromoController() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ZeroStatePromoController)

void ZeroStatePromoController::BindInterface(
    mojo::PendingReceiver<zero_state_promo::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ZeroStatePromoController::BindInterface(
    mojo::PendingReceiver<
        custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory>
        pending_receiver) {
  CustomWebUIHelpBubbleController::BindInterface(std::move(pending_receiver));
}

void ZeroStatePromoController::CreatePageHandler(
    mojo::PendingReceiver<zero_state_promo::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<ZeroStatePromoPageHandler>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}

}  // namespace extensions
