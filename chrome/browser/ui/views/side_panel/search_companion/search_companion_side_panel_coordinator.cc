// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"

SearchCompanionSidePanelCoordinator::SearchCompanionSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<SearchCompanionSidePanelCoordinator>(*browser),
      browser_(browser),
      // TODO(b/269331995): Localize menu item label.
      name_(u"Companion"),
      icon_(kJourneysIcon) {
  browser_->tab_strip_model()->AddObserver(this);
}

SearchCompanionSidePanelCoordinator::~SearchCompanionSidePanelCoordinator() =
    default;

// static
bool SearchCompanionSidePanelCoordinator::IsSupported(Profile* profile) {
  return !profile->IsIncognitoProfile() && !profile->IsGuestSession();
}

void SearchCompanionSidePanelCoordinator::
    CreateAndRegisterEntriesForExistingWebContents(
        TabStripModel* tab_strip_model) {
  for (int index = 0; index < tab_strip_model->GetTabCount(); index++) {
    auto* contextual_registry =
        SidePanelRegistry::Get(tab_strip_model->GetWebContentsAt(index));
    contextual_registry->Register(CreateCompanionEntry());
  }
}

std::unique_ptr<views::View>
SearchCompanionSidePanelCoordinator::CreateCompanionWebView() {
  auto wrapper =
      std::make_unique<BubbleContentsWrapperT<CompanionSidePanelUntrustedUI>>(
          GURL(chrome::kChromeUIUntrustedCompanionSidePanelURL),
          GetBrowserView()->GetProfile(),
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false);
  auto* raw_wrapper = wrapper.get();
  auto companion_web_view =
      std::make_unique<SidePanelWebUIViewT<CompanionSidePanelUntrustedUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::move(wrapper));

  // Observe on the webcontents for opening links in new tab.
  Observe(companion_web_view->GetWebContents());

  // Need to set browser after SidePanelWebUIViewT is constructed since it
  // creates the WebUIController. The WebUI needs a Browser pointer in order
  // to observe changes to the tab strip model.
  raw_wrapper->GetWebUIController()->GetWeakPtr()->set_browser(browser_);
  return companion_web_view;
}

bool SearchCompanionSidePanelCoordinator::Show() {
  auto* browser_view = GetBrowserView();
  if (!browser_view) {
    return false;
  }

  if (auto* side_panel_coordinator = browser_view->side_panel_coordinator()) {
    side_panel_coordinator->Show(SidePanelEntry::Id::kSearchCompanion);
  }

  return true;
}

BrowserView* SearchCompanionSidePanelCoordinator::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(&GetBrowser());
}

void SearchCompanionSidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::Type::kInserted) {
    for (const auto& inserted_tab : change.GetInsert()->contents) {
      auto* contextual_registry = SidePanelRegistry::Get(inserted_tab.contents);
      if (contextual_registry &&
          !contextual_registry->GetEntryForKey(
              SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion))) {
        contextual_registry->Register(CreateCompanionEntry());
      }
    }
  }
}

std::unique_ptr<SidePanelEntry>
SearchCompanionSidePanelCoordinator::CreateCompanionEntry() {
  return std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kSearchCompanion, name(),
      ui::ImageModel::FromVectorIcon(icon(), ui::kColorIcon,
                                     /*icon_size=*/16),
      base::BindRepeating(
          &SearchCompanionSidePanelCoordinator::CreateCompanionWebView,
          base::Unretained(this)));
}

// This method is called when the WebContents wants to open a link in a new
// tab. This delegate does not override AddNewContents(), so the webcontents
// is not actually created. Instead it forwards the parameters to the real
// browser.
void SearchCompanionSidePanelCoordinator::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  content::OpenURLParams params(url, referrer, disposition, transition,
                                renderer_initiated);

  // If the navigation is initiated by the renderer process, we must set an
  // initiator origin.
  if (renderer_initiated) {
    params.initiator_origin = url::Origin::Create(url);
  }

  // Open the new tab in the foreground.
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  auto* browser_view = GetBrowserView();
  if (!browser_view) {
    return;
  }

  // Open the url in a new tab.
  browser_view->browser()->OpenURL(params);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchCompanionSidePanelCoordinator);
