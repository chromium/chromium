// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/menus/simple_menu_model.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/views/user_education/low_usage_promo.h"
#include "components/plus_addresses/resources/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

ToastService::ToastService(BrowserWindowInterface* browser_window_interface) {
  toast_registry_ = std::make_unique<ToastRegistry>();
  toast_controller_ = std::make_unique<ToastController>(
      browser_window_interface, toast_registry_.get());
  toast_controller_->Init();
  RegisterToasts(browser_window_interface);
}

ToastService::~ToastService() = default;

void ToastService::RegisterToasts(
    BrowserWindowInterface* browser_window_interface) {
  CHECK(toast_registry_->IsEmpty());

  toast_registry_->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(kLinkChromeRefreshIcon,
                                  IDS_LINK_COPIED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kImageCopied,
      ToastSpecification::Builder(kCopyMenuIcon, IDS_IMAGE_COPIED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kLinkToHighlightCopied,
      ToastSpecification::Builder(kLinkChromeRefreshIcon,
                                  IDS_LINK_COPIED_TO_HIGHLIGHT_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kAddedToReadingList,
      ToastSpecification::Builder(kReadingListIcon, IDS_READING_LIST_TOAST_BODY)
          .AddActionButton(IDS_READING_LIST_TOAST_BUTTON,
                           base::BindRepeating(
                               [](BrowserWindowInterface* window) {
                                 window->GetFeatures().side_panel_ui()->Show(
                                     SidePanelEntryId::kReadingList,
                                     SidePanelOpenTrigger::kReadingListToast);
                               },
                               base::Unretained(browser_window_interface)))
          .AddCloseButton()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kClearBrowsingData,
      ToastSpecification::Builder(kTrashCanRefreshIcon,
                                  IDS_CLEAR_BROWSING_DATA_TOAST_BODY)
          .Build());

  // TODO(crbug.com/357930023): This registration only partially implements the
  // non-milestone update toast for testing purposes and will need to be
  // updated.
  toast_registry_->RegisterToast(
      ToastId::kNonMilestoneUpdate,
      ToastSpecification::Builder(kLinkChromeRefreshIcon,
                                  IDS_LINK_COPIED_TOAST_BODY)
          .AddGlobalScoped()
          .Build());

  if (base::FeatureList::IsEnabled(commerce::kProductSpecifications) &&
      base::FeatureList::IsEnabled(commerce::kCompareConfirmationToast)) {
    toast_registry_->RegisterToast(
        ToastId::kAddedToComparisonTable,
        ToastSpecification::Builder(omnibox::kProductSpecificationsAddedIcon,
                                    IDS_COMPARE_PAGE_ACTION_ADDED)
            .AddCloseButton()
            .AddActionButton(IDS_COMPARE_ADDED_TO_TABLE_TOAST_ACTION_BUTTON,
                             base::BindRepeating(
                                 [](BrowserWindowInterface* window) {
                                   window->GetActiveTabInterface()
                                       ->GetTabFeatures()
                                       ->commerce_ui_tab_helper()
                                       ->OnOpenComparePageClicked();
                                 },
                                 base::Unretained(browser_window_interface)))
            .Build());
  }

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled) &&
      base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressFullFormFill)) {
    toast_registry_->RegisterToast(
        ToastId::kPlusAddressOverride,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            plus_addresses::kPlusAddressLogoSmallIcon,
#else
            vector_icons::kEmailIcon,
#endif
            IDS_PLUS_ADDRESS_FULL_FORM_FILL_TOAST_MESSAGE)
            .AddMenu()
            .Build());
  }

  // ESB as a synced setting.
  if (base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)) {
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOn,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            vector_icons::kGshieldIcon,
#else
            kSecurityIcon,
#endif
            IDS_SETTINGS_SAFEBROWSING_ENHANCED_ON_TOAST_MESSAGE)
            .AddActionButton(
                IDS_SETTINGS_SETTINGS,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      window->OpenGURL(
                          GURL("chrome://settings/security"),
                          WindowOpenDisposition::NEW_FOREGROUND_TAB);
                    },
                    base::Unretained(browser_window_interface)))
            .AddCloseButton()
            .Build());
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOnWithoutActionButton,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            vector_icons::kGshieldIcon,
#else
            kSecurityIcon,
#endif
            IDS_SETTINGS_SAFEBROWSING_ENHANCED_ON_TOAST_MESSAGE)
            .Build());
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOff,
        ToastSpecification::Builder(
            kInfoIcon, IDS_SETTINGS_SAFEBROWSING_ENHANCED_OFF_TOAST_MESSAGE)
            .AddActionButton(
                IDS_SETTINGS_SAFEBROWSING_TURN_ON_ENHANCED_TOAST_BUTTON,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      Profile* profile = window->GetProfile();
                      if (profile) {
                        profile->GetPrefs()->SetBoolean(
                            prefs::kSafeBrowsingEnhanced, true);
                      }
                    },
                    base::Unretained(browser_window_interface)))
            .AddCloseButton()
            .Build());
  }
}
