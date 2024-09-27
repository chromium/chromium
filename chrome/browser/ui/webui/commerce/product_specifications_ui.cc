// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/commerce/product_specifications_ui.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/commerce_product_specifications_resources.h"
#include "chrome/grit/commerce_product_specifications_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/webui/shopping_service_handler.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace commerce {

ProductSpecificationsUI::ProductSpecificationsUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  if (!shopping_service || !CanLoadProductSpecificationsFullPageUi(
                               shopping_service->GetAccountChecker())) {
    return;
  }
  // Add ThemeSource to serve the chrome logo.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
  // Add SanitizedImageSource to embed images in WebUI.
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  // Set up the chrome://compare source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUICompareHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kCommerceProductSpecificationsResources,
                      kCommerceProductSpecificationsResourcesSize),
      IDR_COMMERCE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_HTML);

  // Set up chrome://compare/disclosure
  source->AddResourcePath(
      "disclosure/",
      IDR_COMMERCE_PRODUCT_SPECIFICATIONS_DISCLOSURE_PRODUCT_SPECIFICATIONS_DISCLOSURE_HTML);
  source->AddResourcePath(
      "disclosure",
      IDR_COMMERCE_PRODUCT_SPECIFICATIONS_DISCLOSURE_PRODUCT_SPECIFICATIONS_DISCLOSURE_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      // Disclosure strings:
      {"acceptDisclosure", IDS_COMPARE_DISCLOSURE_ACCEPT},
      {"declineDisclosure", IDS_COMPARE_DISCLOSURE_DECLINE},
      {"disclosureAboutItem", IDS_COMPARE_DISCLOSURE_ABOUT_AI_ITEM},
      {"disclosureTabItem", IDS_COMPARE_DISCLOSURE_TAB_ITEM},
      {"disclosureAccountItem", IDS_COMPARE_DISCLOSURE_ACCOUNT_ITEM},
      {"disclosureDataItem", IDS_COMPARE_DISCLOSURE_DATA_ITEM},
      {"disclosureItemsHeader", IDS_COMPARE_DISCLOSURE_ITEMS_HEADER},
      {"disclosureTitle", IDS_COMPARE_DISCLOSURE_TITLE},

      // Main UI strings:
      {"addNewColumn", IDS_COMPARE_ADD_NEW_COLUMN},
      {"buyingOptions", IDS_SHOPPING_INSIGHTS_BUYING_OPTIONS},
      {"citationA11yLabel", IDS_COMPARE_CITATION_A11Y_LABEL},
      {"compareErrorDescription", IDS_COMPARE_ERROR_DESCRIPTION},
      {"compareErrorMessage", IDS_COMPARE_ERROR_TITLE},
      {"compareSyncButton", IDS_COMPARE_SYNC_PROMO_BUTTON},
      {"compareSyncDescription", IDS_COMPARE_SYNC_PROMO_DESCRIPTION},
      {"compareSyncMessage", IDS_COMPARE_SYNC_PROMO_MESSAGE},
      {"delete", IDS_COMPARE_DELETE},
      {"defaultTableTitle", IDS_COMPARE_DEFAULT_TABLE_TITLE},
      {"emptyMenu", IDS_COMPARE_EMPTY_SELECTION_MENU},
      {"emptyProductSelector", IDS_COMPARE_EMPTY_PRODUCT_SELECTOR},
      {"emptyStateDescription", IDS_COMPARE_EMPTY_STATE_TITLE_DESCRIPTION},
      {"emptyStateTitle", IDS_COMPARE_EMPTY_STATE_TITLE},
      {"errorMessage", IDS_COMPARE_ERROR_DESCRIPTION},
      {"experimentalFeatureDisclaimer", IDS_COMPARE_DISCLAIMER},
      {"learnMore", IDS_COMPARE_LEARN_MORE},
      {"learnMoreA11yLabel", IDS_COMPARE_LEARN_MORE_A11Y_LABEL},
      {"offlineMessage", IDS_COMPARE_OFFLINE_TOAST_MESSAGE},
      {"openProductPage", IDS_COMPARE_OPEN_PRODUCT_PAGE_IN_NEW_TAB},
      {"pageTitle", IDS_COMPARE_DEFAULT_PAGE_TITLE},
      {"priceRowTitle", IDS_COMPARE_PRICE_ROW_TITLE},
      {"productSummaryRowTitle", IDS_COMPARE_PRODUCT_SUMMARY_ROW_TITLE},
      {"recentlyViewedTabs", IDS_COMPARE_RECENTLY_VIEWED_TABS_SECTION},
      {"removeColumn", IDS_COMPARE_REMOVE_COLUMN},
      {"renameGroup", IDS_COMPARE_RENAME},
      {"seeAll", IDS_COMPARE_SEE_ALL},
      {"suggestedTabs", IDS_COMPARE_SUGGESTIONS_SECTION},
      {"tableFullMessage", IDS_COMPARE_TABLE_FULL_MESSAGE},
      {"tableMenuA11yLabel", IDS_COMPARE_TABLE_MENU_A11Y_LABEL},
      {"tableNameInputA11yLabel", IDS_COMPARE_TITLE_INPUT_A11Y_LABEL},
      {"thumbsDown", IDS_THUMBS_DOWN},
      {"thumbsUp", IDS_THUMBS_UP},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddString("productSpecificationsManagementUrl",
                    kChromeUICompareListsUrl);
  source->AddString("compareLearnMoreUrl", kChromeUICompareLearnMoreUrl);
  source->AddInteger("maxNameLength", kMaxNameLength);
  source->AddInteger("maxTableSize", kMaxTableSize);

  std::string email;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager) {
    CoreAccountInfo account_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    email = account_info.email;
  }
  source->AddString("userEmail", email);
}

void ProductSpecificationsUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void ProductSpecificationsUI::BindInterface(
    mojo::PendingReceiver<
        shopping_service::mojom::ShoppingServiceHandlerFactory> receiver) {
  shopping_service_factory_receiver_.reset();
  shopping_service_factory_receiver_.Bind(std::move(receiver));
}

void ProductSpecificationsUI::CreateShoppingServiceHandler(
    mojo::PendingRemote<shopping_service::mojom::Page> page,
    mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
        receiver) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  shopping_service_handler_ =
      std::make_unique<commerce::ShoppingServiceHandler>(
          std::move(page), std::move(receiver), bookmark_model,
          shopping_service, profile->GetPrefs(), tracker,
          std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr,
                                                                profile),
          optimization_guide_keyed_service
              ? optimization_guide_keyed_service
                    ->GetModelQualityLogsUploaderService()
              : nullptr);
}

// static
base::RefCountedMemory* ProductSpecificationsUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_COMMERCE_PRODUCT_SPECIFICATIONS_FAVICON, scale_factor);
}

ProductSpecificationsUI::~ProductSpecificationsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ProductSpecificationsUI)

ProductSpecificationsUIConfig::ProductSpecificationsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, kChromeUICompareHost) {}

bool ProductSpecificationsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  return profile && !profile->IsOffTheRecord();
}

ProductSpecificationsUIConfig::~ProductSpecificationsUIConfig() = default;

}  // namespace commerce
