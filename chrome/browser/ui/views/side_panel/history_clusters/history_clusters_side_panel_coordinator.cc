// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"

#include "base/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

HistoryClustersSidePanelCoordinator::HistoryClustersSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<HistoryClustersSidePanelCoordinator>(*browser) {}

HistoryClustersSidePanelCoordinator::~HistoryClustersSidePanelCoordinator() =
    default;

void HistoryClustersSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kHistoryClusters,
      l10n_util::GetStringUTF16(IDS_HISTORY_CLUSTERS_JOURNEYS_TAB_LABEL),
      ui::ImageModel::FromVectorIcon(kJourneysIcon, ui::kColorIcon),
      base::BindRepeating(
          &HistoryClustersSidePanelCoordinator::CreateHistoryClustersWebView,
          base::Unretained(this))));
}

std::unique_ptr<views::View>
HistoryClustersSidePanelCoordinator::CreateHistoryClustersWebView() {
  return std::make_unique<SidePanelWebUIViewT<HistoryClustersSidePanelUI>>(
      &GetBrowser(), base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<BubbleContentsWrapperT<HistoryClustersSidePanelUI>>(
          GURL(chrome::kChromeUIHistoryClustersSidePanelURL),
          GetBrowser().profile(), IDS_HISTORY_CLUSTERS_JOURNEYS_TAB_LABEL,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryClustersSidePanelCoordinator);
