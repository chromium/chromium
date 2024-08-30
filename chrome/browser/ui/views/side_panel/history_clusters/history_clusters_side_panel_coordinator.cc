// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"

#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
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
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/omnibox/browser/actions/history_clusters_action.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"

using SidePanelWebUIViewT_HistoryClustersSidePanelUI =
    SidePanelWebUIViewT<HistoryClustersSidePanelUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_HistoryClustersSidePanelUI,
                        SidePanelWebUIViewT)
END_METADATA

HistoryClustersSidePanelCoordinator::HistoryClustersSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<HistoryClustersSidePanelCoordinator>(*browser) {
  pref_change_registrar_.Init(browser->profile()->GetPrefs());
  base::RepeatingClosure callback(base::BindRepeating(
      &HistoryClustersSidePanelCoordinator::OnHistoryClustersPreferenceChanged,
      base::Unretained(this)));
  pref_change_registrar_.Add(history_clusters::prefs::kVisible, callback);
}

HistoryClustersSidePanelCoordinator::~HistoryClustersSidePanelCoordinator() =
    default;

// static
bool HistoryClustersSidePanelCoordinator::IsSupported(Profile* profile) {
  return side_panel::history_clusters::
      IsHistoryClustersSidePanelSupportedForProfile(profile);
}

void HistoryClustersSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kHistoryClusters,
      base::BindRepeating(
          &HistoryClustersSidePanelCoordinator::CreateHistoryClustersWebView,
          base::Unretained(this)),
      base::BindRepeating(
          &HistoryClustersSidePanelCoordinator::GetOpenInNewTabURL,
          base::Unretained(this))));
}

std::unique_ptr<views::View>
HistoryClustersSidePanelCoordinator::CreateHistoryClustersWebView() {
  // Construct our URL including our initial query. Other ways of passing the
  // initial query to the WebUI interface are mostly all racy.
  std::string query_string = base::StringPrintf(
      "initial_query=%s",
      base::EscapeQueryParamValue(initial_query_, /*use_plus=*/false).c_str());

  // Side Panel WebViews created from the omnibox have a non-empty query, and
  // ones created from the toolbar have an empty query.
  const auto created_from_omnibox = !initial_query_.empty();

  // Clear the initial query, because this is a singleton, and we want to
  // "consume" the initial query after we create a side panel with it.
  initial_query_.clear();

  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  const GURL url = GURL(chrome::kChromeUIHistoryClustersSidePanelURL)
                       .ReplaceComponents(replacements);

  auto side_panel_ui =
      std::make_unique<SidePanelWebUIViewT<HistoryClustersSidePanelUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<WebUIContentsWrapperT<HistoryClustersSidePanelUI>>(
              url, GetBrowser().profile(), IDS_HISTORY_TITLE,
              /*esc_closes_ui=*/false));
  history_clusters_ui_ =
      side_panel_ui->contents_wrapper()->GetWebUIController()->GetWeakPtr();
  history_clusters_ui_->SetBrowserWindowInterface(&GetBrowser());
  history_clusters_ui_->set_metrics_initial_state(
      created_from_omnibox
          ? history_clusters::HistoryClustersInitialState::kSidePanelFromOmnibox
          : history_clusters::HistoryClustersInitialState::
                kSidePanelFromToolbarButton);

  return std::move(side_panel_ui);
}

void HistoryClustersSidePanelCoordinator::OnHistoryClustersPreferenceChanged() {
  auto* browser = &GetBrowser();
  auto* global_registry = browser->browser_window_features()
                              ->side_panel_coordinator()
                              ->GetWindowRegistry();
  if (IsSupported(browser->profile())) {
    HistoryClustersSidePanelCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  } else {
    global_registry->Deregister(
        SidePanelEntry::Key(SidePanelEntry::Id::kHistoryClusters));
  }
}

bool HistoryClustersSidePanelCoordinator::Show(const std::string& query) {
  SidePanelUI* side_panel_ui = GetBrowser().GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return false;
  }

  if (history_clusters_ui_) {
    history_clusters_ui_->SetQuery(query);
  } else {
    // If the WebUI hasn't been created yet, store it in a member, so that when
    // it is created (by a different callsite) it receives the initial query.
    initial_query_ = query;
  }

  side_panel_ui->Show(SidePanelEntry::Id::kHistoryClusters);

  return true;
}

GURL HistoryClustersSidePanelCoordinator::GetOpenInNewTabURL() const {
  std::string query;
  if (history_clusters_ui_)
    query = history_clusters_ui_->GetLastQueryIssued();

  return query.empty() ? GURL(history_clusters::GetChromeUIHistoryClustersURL())
                       : history_clusters::GetFullJourneysUrlForQuery(query);
}

BROWSER_USER_DATA_KEY_IMPL(HistoryClustersSidePanelCoordinator);
