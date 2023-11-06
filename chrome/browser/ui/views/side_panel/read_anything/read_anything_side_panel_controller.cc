// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_container_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/grit/generated_resources.h"
#include "read_anything_controller.h"
#include "read_anything_coordinator.h"
#include "read_anything_side_panel_controller.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_types.h"

using SidePanelWebUIViewT_ReadAnythingUntrustedUI =
    SidePanelWebUIViewT<ReadAnythingUntrustedUI>;
DECLARE_TEMPLATE_METADATA(SidePanelWebUIViewT_ReadAnythingUntrustedUI,
                          SidePanelWebUIViewT);

ReadAnythingSidePanelController::ReadAnythingSidePanelController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ReadAnythingSidePanelController::~ReadAnythingSidePanelController() = default;

void ReadAnythingSidePanelController::CreateAndRegisterEntry() {
  auto* registry = SidePanelRegistry::Get(web_contents_);
  if (!registry || registry->GetEntryForKey(SidePanelEntry::Key(
                       SidePanelEntry::Id::kReadAnything))) {
    return;
  }

  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE),
      ui::ImageModel::FromVectorIcon(kMenuBookChromeRefreshIcon,
                                     ui::kColorIcon),
      base::BindRepeating(&ReadAnythingSidePanelController::CreateContainerView,
                          base::Unretained(this)));
  side_panel_entry->AddObserver(this);
  registry->Register(std::move(side_panel_entry));
}

void ReadAnythingSidePanelController::DeregisterEntry() {
  auto* registry = SidePanelRegistry::Get(web_contents_);
  if (!registry) {
    return;
  }

  if (auto* current_entry = registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))) {
    current_entry->RemoveObserver(this);
  }
  registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
}

void ReadAnythingSidePanelController::OnEntryShown(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents_)) {
    auto* coordinator = ReadAnythingCoordinator::GetOrCreateForBrowser(browser);
    coordinator->OnReadAnythingSidePanelEntryShown();
  }
}

void ReadAnythingSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents_)) {
    auto* coordinator = ReadAnythingCoordinator::GetOrCreateForBrowser(browser);
    coordinator->OnReadAnythingSidePanelEntryHidden();
  }
}

std::unique_ptr<views::View>
ReadAnythingSidePanelController::CreateContainerView() {
  auto web_view =
      std::make_unique<SidePanelWebUIViewT<ReadAnythingUntrustedUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<BubbleContentsWrapperT<ReadAnythingUntrustedUI>>(
              GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL),
              Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
              IDS_READING_MODE_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false));

  if (features::IsReadAnythingWebUIToolbarEnabled()) {
    return std::move(web_view);
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    // If no browser was found via WebContents, it is probably because the
    // web_contents has not been attached to a window yet. Since we are only
    // using the browser to find the ReadAnythingCoordinator, it is safe to grab
    // the LastActive browser as a fallback.
    browser = chrome::FindLastActive();
  }

  CHECK(browser);
  auto* coordinator = ReadAnythingCoordinator::GetOrCreateForBrowser(browser);

  // Create the views.
  auto toolbar = std::make_unique<ReadAnythingToolbarView>(
      coordinator,
      /*toolbar_delegate=*/coordinator->GetController(),
      /*font_combobox_delegate=*/coordinator->GetController());

  // Create the component.
  // Note that a coordinator would normally maintain ownership of these objects,
  // but objects extending {ui/views/view.h} prefer ownership over raw pointers
  // (View ownership is typically managed by the View hierarchy, rather than by
  // outside coordinators).
  auto container_view = std::make_unique<ReadAnythingContainerView>(
      coordinator, std::move(toolbar), std::move(web_view));

  return std::move(container_view);
}
