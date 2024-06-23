// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include <algorithm>
#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/common/locale_util.h"
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

ReadAnythingSidePanelController::~ReadAnythingSidePanelController() {
  // Inform observers when |this| is destroyed so they can do their own cleanup.
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.OnSidePanelControllerDestroyed();
  }
}

void ReadAnythingSidePanelController::CreateAndRegisterEntry() {
  auto* registry = SidePanelRegistry::Get(web_contents_);
  if (!registry || registry->GetEntryForKey(SidePanelEntry::Key(
                       SidePanelEntry::Id::kReadAnything))) {
    return;
  }

  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
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

void ReadAnythingSidePanelController::AddPageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  AddObserver(page_handler.get());
}

void ReadAnythingSidePanelController::RemovePageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  RemoveObserver(page_handler.get());
}

void ReadAnythingSidePanelController::AddObserver(
    ReadAnythingSidePanelController::Observer* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingSidePanelController::RemoveObserver(
    ReadAnythingSidePanelController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingSidePanelController::OnEntryShown(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents_)) {
    auto* coordinator = ReadAnythingCoordinator::GetOrCreateForBrowser(browser);
    coordinator->OnReadAnythingSidePanelEntryShown();
  }
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.Activate(true);
  }
}

void ReadAnythingSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents_)) {
    auto* coordinator = ReadAnythingCoordinator::GetOrCreateForBrowser(browser);
    coordinator->OnReadAnythingSidePanelEntryHidden();
  }
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.Activate(false);
  }
}

std::unique_ptr<views::View>
ReadAnythingSidePanelController::CreateContainerView() {
  auto web_view = std::make_unique<ReadAnythingSidePanelWebView>(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));

  return std::move(web_view);
}
