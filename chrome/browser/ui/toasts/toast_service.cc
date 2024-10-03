// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
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
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/simple_menu_model.h"

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

  // TODO(crbug.com/357929158): This registration only partially implements the
  // Chromnient toast and will need to handle alternate icons and strings.
  toast_registry_->RegisterToast(
      ToastId::kLensOverlay,
      ToastSpecification::Builder(vector_icons::kSearchChromeRefreshIcon,
                                  IDS_LENS_OVERLAY_INITIAL_TOAST_MESSAGE)
          .AddPersistance()
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
}
