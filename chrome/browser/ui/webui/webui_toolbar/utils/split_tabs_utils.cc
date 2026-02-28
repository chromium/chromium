/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_util.h"

namespace webui_toolbar {

TabSplitStatus ComputeTabSplitStatus(
    BrowserWindowInterface* browser_interface) {
  TabSplitStatus status;

  // Tab state and split detection.
  auto* tab_strip_model = browser_interface->GetTabStripModel();
  if (tab_strip_model && tab_strip_model->GetActiveTab()) {
    status.is_split = tab_strip_model->GetActiveTab()->IsSplit();
  }

  if (status.is_split) {
    // If split, determine which side is active.
    auto location = split_tabs::GetLastActiveTabLocation(
        tab_strip_model, tab_strip_model->GetActiveTab()->GetSplit().value());

    switch (location) {
      case split_tabs::SplitTabActiveLocation::kStart:
        status.location = toolbar_ui_api::mojom::SplitTabActiveLocation::kStart;
        break;
      case split_tabs::SplitTabActiveLocation::kEnd:
        status.location = toolbar_ui_api::mojom::SplitTabActiveLocation::kEnd;
        break;
      case split_tabs::SplitTabActiveLocation::kTop:
        status.location = toolbar_ui_api::mojom::SplitTabActiveLocation::kTop;
        break;
      case split_tabs::SplitTabActiveLocation::kBottom:
        status.location =
            toolbar_ui_api::mojom::SplitTabActiveLocation::kBottom;
        break;
    }
  }

  return status;
}

bool IsButtonPinned(BrowserWindowInterface* browser_interface,
                    toolbar_ui_api::mojom::ToolbarButtonType type) {
  switch (type) {
    case toolbar_ui_api::mojom::ToolbarButtonType::kSplitTabs:
      return browser_interface->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kPinSplitTabButton);
    default:
      return false;
  }
}

void PopulateSplitTabsDataSource(content::WebUIDataSource* source,
                                 BrowserWindowInterface* browser_interface) {
  source->AddBoolean("enableSplitTabsButton",
                     features::IsWebUISplitTabsButtonEnabled());

  static constexpr webui::LocalizedString kStrings[] = {
      {"splitTabsButtonAccNameEnabled",
       IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_ENABLED},
      {"splitTabsButtonAccNamePinned",
       IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_PINNED},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddInteger("splitTabsIndicatorWidth", kSplitTabsStatusIndicatorWidth);
  source->AddInteger("splitTabsIndicatorHeight",
                     kSplitTabsStatusIndicatorHeight);
  source->AddInteger("splitTabsIndicatorSpacing",
                     kSplitTabsStatusIndicatorSpacing);
}

}  // namespace webui_toolbar
