// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"

#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_utils.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/history/history_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"

using SidePanelWebUIViewT_HistorySidePanelUI =
    SidePanelWebUIViewT<HistorySidePanelUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_HistorySidePanelUI,
                        SidePanelWebUIViewT)
END_METADATA

HistorySidePanelCoordinator::HistorySidePanelCoordinator(
    BrowserWindowInterface* browser)
    : browser_(browser) {
  pref_change_registrar_.Init(browser->GetProfile()->GetPrefs());
  base::RepeatingClosure callback(base::BindRepeating(
      &HistorySidePanelCoordinator::OnHistoryClustersPreferenceChanged,
      base::Unretained(this)));
  pref_change_registrar_.Add(history_clusters::prefs::kVisible, callback);
}

// static
bool HistorySidePanelCoordinator::IsSupported() {
  return base::FeatureList::IsEnabled(features::kByDateHistoryInSidePanel);
}

void HistorySidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kHistory),
      base::BindRepeating(&HistorySidePanelCoordinator::CreateHistoryWebView,
                          base::Unretained(this)),
      SidePanelEntry::kSidePanelDefaultContentWidth));
}

std::unique_ptr<views::View> HistorySidePanelCoordinator::CreateHistoryWebView(
    SidePanelEntryScope& scope) {
  // Construct our URL including our initial query. Other ways of passing the
  // initial query to the WebUI interface are mostly all racy.
  std::string query_string = base::StringPrintf(
      "initial_query=%s",
      base::EscapeQueryParamValue(initial_query_, /*use_plus=*/false).c_str());

  // Clear the initial query, because this is a singleton, and we want to
  // "consume" the initial query after we create a side panel with it.
  initial_query_.clear();

  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  const GURL url = GURL(chrome::kChromeUIHistorySidePanelURL)
                       .ReplaceComponents(replacements);

  return std::make_unique<SidePanelWebUIViewT<HistorySidePanelUI>>(
      scope, base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<WebUIContentsWrapperT<HistorySidePanelUI>>(
          url, browser_->GetProfile(), IDS_HISTORY_TITLE,
          /*esc_closes_ui=*/false));
}

void HistorySidePanelCoordinator::OnHistoryClustersPreferenceChanged() {
  // TODO(mfacey): crbug.com/399683127 Hide or show history clusters based on
  // pref val.
}

bool HistorySidePanelCoordinator::Show(const std::string& query) {
  SidePanelUI* side_panel_ui = browser_->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return false;
  }

  side_panel_ui->Show(SidePanelEntry::Id::kHistory);

  return true;
}
