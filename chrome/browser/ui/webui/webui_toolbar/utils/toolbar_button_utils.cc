// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_ids.h"
#include "chrome/common/pref_names.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_service.h"

namespace webui_toolbar {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarClearBrowsingDataElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarCopyUrlElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarDevToolsElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarNewIncognitoWindowElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarPrintElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarQrCodeGeneratorElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarRouteMediaElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSendTabToSelfElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowAddressesBubbleOrPageElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowDownloadsElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowPasswordsBubbleOrPageElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowPaymentsBubbleOrPageElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowTranslateElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowAboutThisSiteElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowContextualTasksElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowCustomizeChromeElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowHistoryClusterElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowLensElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowMerchantTrustElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowReadAnythingElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowReadingListElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kToolbarSidePanelShowShoppingInsightsElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarTabSearchElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarTaskManagerElementId);

DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarClearBrowsingDataElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarCopyUrlElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarDevToolsElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarNewIncognitoWindowElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarPrintElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarQrCodeGeneratorElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarRouteMediaElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSendTabToSelfElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowAddressesBubbleOrPageElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowDownloadsElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowPasswordsBubbleOrPageElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowPaymentsBubbleOrPageElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowTranslateElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowAboutThisSiteElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowContextualTasksElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowCustomizeChromeElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowHistoryClusterElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowLensElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowMerchantTrustElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowReadAnythingElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowReadingListElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarSidePanelShowShoppingInsightsElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarTabSearchElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarTaskManagerElementId);

std::vector<ui::ElementIdentifier> GetPinnedToolbarActionElementIds() {
  return {webui_toolbar::kToolbarClearBrowsingDataElementId,
          webui_toolbar::kToolbarCopyUrlElementId,
          webui_toolbar::kToolbarDevToolsElementId,
          webui_toolbar::kToolbarNewIncognitoWindowElementId,
          webui_toolbar::kToolbarPrintElementId,
          webui_toolbar::kToolbarQrCodeGeneratorElementId,
          webui_toolbar::kToolbarRouteMediaElementId,
          webui_toolbar::kToolbarSendTabToSelfElementId,
          webui_toolbar::kToolbarShowAddressesBubbleOrPageElementId,
          webui_toolbar::kToolbarShowDownloadsElementId,
          webui_toolbar::kToolbarShowPasswordsBubbleOrPageElementId,
          webui_toolbar::kToolbarShowPaymentsBubbleOrPageElementId,
          webui_toolbar::kToolbarShowTranslateElementId,
          webui_toolbar::kToolbarSidePanelShowAboutThisSiteElementId,
          webui_toolbar::kToolbarSidePanelShowContextualTasksElementId,
          webui_toolbar::kToolbarSidePanelShowCustomizeChromeElementId,
          webui_toolbar::kToolbarSidePanelShowHistoryClusterElementId,
          webui_toolbar::kToolbarSidePanelShowLensElementId,
          webui_toolbar::kToolbarSidePanelShowMerchantTrustElementId,
          webui_toolbar::kToolbarSidePanelShowReadAnythingElementId,
          webui_toolbar::kToolbarSidePanelShowReadingListElementId,
          webui_toolbar::kToolbarSidePanelShowShoppingInsightsElementId,
          webui_toolbar::kToolbarTabSearchElementId,
          webui_toolbar::kToolbarTaskManagerElementId};
}

bool IsButtonPinned(BrowserWindowInterface* browser_interface,
                    toolbar_ui_api::mojom::ToolbarButtonType type) {
  switch (type) {
    case toolbar_ui_api::mojom::ToolbarButtonType::kSplitTabs:
      return browser_interface->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kPinSplitTabButton);
    case toolbar_ui_api::mojom::ToolbarButtonType::kHome:
      return browser_interface->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kShowHomeButton);
    case toolbar_ui_api::mojom::ToolbarButtonType::kUnspecified:
      NOTREACHED() << "Unexpected ToolbarButtonType::kUnspecified.";
  }
}

ui::ElementIdentifier ActionIdToElementIdentifier(actions::ActionId action) {
  if (auto id = pinned_toolbar_actions::GetElementIdentifierForAction(action)) {
    return id;
  }
  switch (action) {
    case kActionNewIncognitoWindow:
      return kToolbarNewIncognitoWindowElementId;
    case kActionShowPasswordsBubbleOrPage:
      return kToolbarShowPasswordsBubbleOrPageElementId;
    case kActionShowPaymentsBubbleOrPage:
      return kToolbarShowPaymentsBubbleOrPageElementId;
    case kActionShowAddressesBubbleOrPage:
      return kToolbarShowAddressesBubbleOrPageElementId;
    case kActionSidePanelShowReadingList:
      return kToolbarSidePanelShowReadingListElementId;
    case kActionSidePanelShowHistoryCluster:
      return kToolbarSidePanelShowHistoryClusterElementId;
    case kActionShowDownloads:
      return kToolbarShowDownloadsElementId;
    case kActionClearBrowsingData:
      return kToolbarClearBrowsingDataElementId;
    case kActionPrint:
      return kToolbarPrintElementId;
    case kActionShowTranslate:
      return kToolbarShowTranslateElementId;
    case kActionQrCodeGenerator:
      return kToolbarQrCodeGeneratorElementId;
    case kActionRouteMedia:
      return kToolbarRouteMediaElementId;
    case kActionSidePanelShowReadAnything:
      return kToolbarSidePanelShowReadAnythingElementId;
    case kActionCopyUrl:
      return kToolbarCopyUrlElementId;
    case kActionSendTabToSelf:
      return kToolbarSendTabToSelfElementId;
    case kActionTaskManager:
      return kToolbarTaskManagerElementId;
    case kActionDevTools:
      return kToolbarDevToolsElementId;
    case kActionTabSearch:
      return kToolbarTabSearchElementId;
    case kActionSidePanelShowContextualTasks:
      return kToolbarSidePanelShowContextualTasksElementId;
    case kActionSidePanelShowLens:
      return kToolbarSidePanelShowLensElementId;
    case kActionSidePanelShowAboutThisSite:
      return kToolbarSidePanelShowAboutThisSiteElementId;
    case kActionSidePanelShowCustomizeChrome:
      return kToolbarSidePanelShowCustomizeChromeElementId;
    case kActionSidePanelShowShoppingInsights:
      return kToolbarSidePanelShowShoppingInsightsElementId;
    case kActionSidePanelShowMerchantTrust:
      return kToolbarSidePanelShowMerchantTrustElementId;
    default:
      return ui::ElementIdentifier();
  }
}

std::optional<toolbar_ui_api::mojom::PinnedToolbarAction>
ActionIdToPinnedToolbarAction(actions::ActionId action) {
  switch (action) {
    case kActionNewIncognitoWindow:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kNewIncognitoWindow;
    case kActionShowPasswordsBubbleOrPage:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowPasswordsBubbleOrPage;
    case kActionShowPaymentsBubbleOrPage:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowPaymentsBubbleOrPage;
    case kActionShowAddressesBubbleOrPage:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowAddressesBubbleOrPage;
    case kActionSidePanelShowBookmarks:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowBookmarks;
    case kActionSidePanelShowReadingList:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowReadingList;
    case kActionSidePanelShowHistoryCluster:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowHistoryCluster;
    case kActionShowDownloads:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kShowDownloads;
    case kActionClearBrowsingData:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kClearBrowsingData;
    case kActionPrint:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;
    case kActionSidePanelShowLensOverlayResults:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowLensOverlayResults;
    case kActionShowTranslate:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate;
    case kActionQrCodeGenerator:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kQrCodeGenerator;
    case kActionRouteMedia:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia;
    case kActionSidePanelShowReadAnything:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowReadAnything;
    case kActionCopyUrl:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl;
    case kActionSendTabToSelf:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kSendTabToSelf;
    case kActionTaskManager:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kTaskManager;
    case kActionDevTools:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kDevTools;
    case kActionTabSearch:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kTabSearch;
    case kActionSidePanelShowContextualTasks:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowContextualTasks;
    case kActionSidePanelShowLens:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowLens;
    case kActionSidePanelShowAboutThisSite:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowAboutThisSite;
    case kActionSidePanelShowCustomizeChrome:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowCustomizeChrome;
    case kActionSidePanelShowShoppingInsights:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowShoppingInsights;
    case kActionSidePanelShowMerchantTrust:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowMerchantTrust;
    case kActionSendSharedTabGroupFeedback:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSendSharedTabGroupFeedback;
    case kActionSidePanelShowComments:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowComments;
    default:
      return std::nullopt;
  }
}

std::optional<actions::ActionId> PinnedToolbarActionToActionId(
    toolbar_ui_api::mojom::PinnedToolbarAction action) {
  switch (action) {
    case toolbar_ui_api::mojom::PinnedToolbarAction::kUnspecified:
      return std::nullopt;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kNewIncognitoWindow:
      return kActionNewIncognitoWindow;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowPasswordsBubbleOrPage:
      return kActionShowPasswordsBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowPaymentsBubbleOrPage:
      return kActionShowPaymentsBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowAddressesBubbleOrPage:
      return kActionShowAddressesBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowBookmarks:
      return kActionSidePanelShowBookmarks;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowReadingList:
      return kActionSidePanelShowReadingList;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowHistoryCluster:
      return kActionSidePanelShowHistoryCluster;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowDownloads:
      return kActionShowDownloads;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kClearBrowsingData:
      return kActionClearBrowsingData;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kPrint:
      return kActionPrint;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowLensOverlayResults:
      return kActionSidePanelShowLensOverlayResults;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate:
      return kActionShowTranslate;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kQrCodeGenerator:
      return kActionQrCodeGenerator;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia:
      return kActionRouteMedia;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowReadAnything:
      return kActionSidePanelShowReadAnything;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl:
      return kActionCopyUrl;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSendTabToSelf:
      return kActionSendTabToSelf;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kTaskManager:
      return kActionTaskManager;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kDevTools:
      return kActionDevTools;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kTabSearch:
      return kActionTabSearch;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowContextualTasks:
      return kActionSidePanelShowContextualTasks;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowLens:
      return kActionSidePanelShowLens;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowAboutThisSite:
      return kActionSidePanelShowAboutThisSite;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowCustomizeChrome:
      return kActionSidePanelShowCustomizeChrome;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowShoppingInsights:
      return kActionSidePanelShowShoppingInsights;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowMerchantTrust:
      return kActionSidePanelShowMerchantTrust;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSendSharedTabGroupFeedback:
      return kActionSendSharedTabGroupFeedback;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowComments:
      return kActionSidePanelShowComments;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kDivider:
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace webui_toolbar
