// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/glic/glic_legacy_side_panel_coordinator.h"

#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/actions.h"

namespace glic {

namespace {

actions::ActionItem* GetGlicActionItem(actions::ActionItem* root_action_item) {
  actions::ActionItem* glic_action_item =
      actions::ActionManager::Get().FindAction(kActionSidePanelShowGlic,
                                               root_action_item);
  CHECK(glic_action_item);
  return glic_action_item;
}

}  // namespace

GlicLegacySidePanelCoordinator::GlicLegacySidePanelCoordinator(Browser* browser)
    : browser_(browser),
      glic_service_(
          GlicKeyedServiceFactory::GetGlicKeyedService(browser->GetProfile())),
      glic_action_(
          GetGlicActionItem(browser->GetActions()->root_action_item())) {
  DCHECK(!GlicEnabling::IsMultiInstanceEnabled());

  on_glic_enabled_changed_subscription_ =
      glic_service_->enabling().RegisterAllowedChanged(base::BindRepeating(
          &GlicLegacySidePanelCoordinator::OnGlicEnabledChanged,
          base::Unretained(this)));
}

void GlicLegacySidePanelCoordinator::CreateAndRegisterEntry(
    Browser* browser,
    SidePanelRegistry* global_registry) {
  if (global_registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kGlic))) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic),
      base::BindRepeating(&GlicLegacySidePanelCoordinator::CreateGlicWebView,
                          base::Unretained(this), browser),
      /*default_content_width_callback=*/base::NullCallback());
  entry->AddObserver(this);
  global_registry->Register(std::move(entry));
}

void GlicLegacySidePanelCoordinator::OnEntryShown(SidePanelEntry* entry) {
  SidePanelEntry::Key glic_key = SidePanelEntry::Key(SidePanelEntry::Id::kGlic);
  if (browser_->GetFeatures().side_panel_ui()->IsSidePanelEntryShowing(
          glic_key)) {
    glic_service_->GetSingleInstanceWindowController().SidePanelShown(browser_);
  }
}

void GlicLegacySidePanelCoordinator::OnGlicEnabledChanged() {
  bool isAllowed =
      glic::GlicEnabling::IsEnabledForProfile(browser_->GetProfile());
  // Show / hide browser action.
  glic_action_->SetVisible(isAllowed);
  // Register / deregister side panel entry.
  SidePanelRegistry* global_registry = SidePanelRegistry::From(browser_);
  if (isAllowed) {
    CreateAndRegisterEntry(browser_, global_registry);
  } else {
    SidePanelEntry::Key glic_key =
        SidePanelEntry::Key(SidePanelEntry::Id::kGlic);
    SidePanelEntry* const glic_entry =
        global_registry->GetEntryForKey(glic_key);
    if (glic_entry) {
      glic_entry->RemoveObserver(this);
      SidePanelUI* const side_panel_ui =
          browser_->GetFeatures().side_panel_ui();
      if (side_panel_ui->IsSidePanelEntryShowing(glic_key)) {
        side_panel_ui->Close(glic_entry->type());
      }
    }
    global_registry->Deregister(glic_key);
  }
}

std::unique_ptr<views::View> GlicLegacySidePanelCoordinator::CreateGlicWebView(
    Browser* browser,
    SidePanelEntryScope& scope) {
  auto* tab = scope.GetBrowserWindowInterface().GetActiveTabInterface();
  if (!tab) {
    return nullptr;
  }
  return glic_service_->GetSingleInstanceWindowController()
      .CreateViewForSidePanel(
          *scope.GetBrowserWindowInterface().GetActiveTabInterface());
}

}  // namespace glic
