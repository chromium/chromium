// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include <algorithm>
#include <memory>

#include "base/check_is_test.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
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
    tabs::TabInterface* tab,
    SidePanelRegistry* side_panel_registry)
    : tab_(tab), side_panel_registry_(side_panel_registry) {
  CHECK(!side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything)));

  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      base::BindRepeating(&ReadAnythingSidePanelController::CreateContainerView,
                          base::Unretained(this)));
  side_panel_entry->AddObserver(this);
  side_panel_registry_->Register(std::move(side_panel_entry));

  tab_subscriptions_.push_back(tab_->RegisterWillDetach(
      base::BindRepeating(&ReadAnythingSidePanelController::TabWillDetach,
                          weak_factory_.GetWeakPtr())));
}

ReadAnythingSidePanelController::~ReadAnythingSidePanelController() {
  // Inform observers when |this| is destroyed so they can do their own cleanup.
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.OnSidePanelControllerDestroyed();
  }
}

void ReadAnythingSidePanelController::ResetForTabDiscard() {
  auto* current_entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  current_entry->RemoveObserver(this);
  side_panel_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
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
  auto* coordinator = tab_->GetBrowserWindowInterface()
                          ->GetFeatures()
                          .read_anything_coordinator();
  coordinator->OnReadAnythingSidePanelEntryShown();
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.Activate(true);
  }
}

void ReadAnythingSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  auto* coordinator = tab_->GetBrowserWindowInterface()
                          ->GetFeatures()
                          .read_anything_coordinator();
  coordinator->OnReadAnythingSidePanelEntryHidden();
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.Activate(false);
  }
}

std::unique_ptr<views::View>
ReadAnythingSidePanelController::CreateContainerView() {
  auto web_view = std::make_unique<ReadAnythingSidePanelWebView>(
      tab_->GetBrowserWindowInterface()->GetProfile());

  return std::move(web_view);
}

void ReadAnythingSidePanelController::TabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  // TODO(https://crbug.com/360169305): TabStripModel unit tests currently
  // create TabFeatures but are not associated with a Browser, which is
  // conceptually incorrect.
  if (!tab_->GetBrowserWindowInterface()) {
    CHECK_IS_TEST();
    return;  // IN-TEST
  }

  auto* coordinator =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_coordinator();
  // TODO(https://crbug.com/360163254): BrowserWithTestWindowTest currently does
  // not create a SidePanelCoordinator. This block will be unnecessary once that
  // changes.
  if (!coordinator) {
    CHECK_IS_TEST();
    return;  // IN-TEST
  }
  if (coordinator->IsSidePanelEntryShowing(
          SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))) {
    coordinator->Close(/*suppress_animation=*/true);
  }
}
