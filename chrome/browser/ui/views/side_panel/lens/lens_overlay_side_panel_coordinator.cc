// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_overlay_side_panel_coordinator.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/lens/lens_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/vector_icons.h"

using SidePanelWebUIViewT_LensUntrustedUI =
    SidePanelWebUIViewT<lens::LensUntrustedUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_LensUntrustedUI,
                        SidePanelWebUIViewT)
END_METADATA

namespace lens {

LensOverlaySidePanelCoordinator::LensOverlaySidePanelCoordinator(
    Browser* browser,
    LensOverlayController* lens_overlay_controller,
    SidePanelUI* side_panel_ui,
    content::WebContents* web_contents)
    : tab_browser_(browser),
      lens_overlay_controller_(lens_overlay_controller),
      side_panel_ui_(side_panel_ui),
      tab_web_contents_(web_contents->GetWeakPtr()) {}

LensOverlaySidePanelCoordinator::~LensOverlaySidePanelCoordinator() {
  DeregisterEntry();
}

void LensOverlaySidePanelCoordinator::RegisterEntryAndShow() {
  RegisterEntry();
  side_panel_ui_->Show(SidePanelEntry::Id::kLensOverlayResults);
}

void LensOverlaySidePanelCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  DeregisterEntry();
}

void LensOverlaySidePanelCoordinator::RegisterEntry() {
  auto* registry = SidePanelRegistry::Get(GetTabWebContents());
  CHECK(registry);

  // If the entry is already registered, don't register it again.
  if (!registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults))) {
    // TODO(b/328295358): Change title and icon when available.
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLensOverlayResults,
        l10n_util::GetStringUTF16(IDS_SIDE_PANEL_COMPANION_TITLE),
        ui::ImageModel::FromVectorIcon(vector_icons::kSearchIcon,
                                       ui::kColorIcon,
                                       /*icon_size=*/16),
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView,
            base::Unretained(this)),
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::GetOpenInNewTabUrl,
            base::Unretained(this)));
    registry->Register(std::move(entry));

    // Observe the side panel entry.
    auto* registered_entry = registry->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
    registered_entry->AddObserver(this);
  }
}

void LensOverlaySidePanelCoordinator::DeregisterEntry() {
  auto* registry = SidePanelRegistry::Get(GetTabWebContents());
  CHECK(registry);
  // If the side panel web view was created, then we need to remove the glue to
  // the overlay controller if it is present.
  if (side_panel_web_view_) {
    lens_overlay_controller_->RemoveGlueForWebView(side_panel_web_view_);
    side_panel_web_view_ = nullptr;
  }

  // Remove the side panel entry observer if it is present.
  auto* registered_entry = registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
  if (registered_entry) {
    registered_entry->RemoveObserver(this);
  }

  // This is a no-op if the entry does not exist.
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
}

std::unique_ptr<views::View>
LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView() {
  // TODO(b/328295358): Change task manager string ID in view creation when
  // available.
  auto view = std::make_unique<SidePanelWebUIViewT<lens::LensUntrustedUI>>(
      base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<WebUIContentsWrapperT<lens::LensUntrustedUI>>(
          GURL(chrome::kChromeUILensUntrustedSidePanelURL),
          tab_browser_->profile(), IDS_SIDE_PANEL_COMPANION_TITLE,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false));
  side_panel_web_view_ = view.get();
  // Important safety note: creating the SidePanelWebUIViewT can result in
  // synchronous construction of the WebUIController. Until
  // "CreateGlueForWebView" is called below, the WebUIController will not be
  // able to access to LensOverlayController.
  lens_overlay_controller_->CreateGlueForWebView(view.get());
  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}

GURL LensOverlaySidePanelCoordinator::GetOpenInNewTabUrl() {
  return GURL();
}

content::WebContents* LensOverlaySidePanelCoordinator::GetTabWebContents() {
  content::WebContents* tab_contents = tab_web_contents_.get();
  CHECK(tab_contents);
  return tab_contents;
}

}  // namespace lens
