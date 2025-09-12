// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"

#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "components/tabs/public/tab_interface.h"
#include "glic_side_panel_coordinator.h"
#include "ui/actions/actions.h"

namespace glic {
DEFINE_USER_DATA(GlicSidePanelCoordinator);

namespace {

actions::ActionItem* GetGlicActionItem(actions::ActionItem* root_action_item) {
  actions::ActionItem* glic_action_item =
      actions::ActionManager::Get().FindAction(kActionSidePanelShowGlic,
                                               root_action_item);
  CHECK(glic_action_item);
  return glic_action_item;
}

}  // namespace

GlicSidePanelCoordinator::GlicSidePanelCoordinator(
    tabs::TabInterface* tab,
    SidePanelRegistry* side_panel_registry)
    : tab_(tab),
      side_panel_registry_(side_panel_registry),
      glic_action_(GetGlicActionItem(
          tab->GetBrowserWindowInterface()->GetActions()->root_action_item())),
      side_panel_coordinator_(tab->GetBrowserWindowInterface()
                                  ->GetFeatures()
                                  .side_panel_coordinator()) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicMultiInstance));
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      tab->GetBrowserWindowInterface()->GetProfile());
  if (glic_service) {
    on_glic_enabled_changed_subscription_ =
        glic_service->enabling().RegisterAllowedChanged(
            base::BindRepeating(&GlicSidePanelCoordinator::OnGlicEnabledChanged,
                                base::Unretained(this)));
    if (glic_service->enabling().IsAllowed()) {
      CreateAndRegisterEntry();
    }
  }
}

GlicSidePanelCoordinator::~GlicSidePanelCoordinator() = default;

void GlicSidePanelCoordinator::CreateAndRegisterEntry() {
  if (side_panel_registry_->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kGlic))) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic),
      base::BindRepeating(&GlicSidePanelCoordinator::CreateView,
                          base::Unretained(this)),
      /*default_content_width_callback=*/base::NullCallback());
  entry->AddObserver(this);
  side_panel_registry_->Register(std::move(entry));
}

void GlicSidePanelCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kGlic);
  state_observers_.Notify(&StateObserver::VisibilityChanged, false);
}

void GlicSidePanelCoordinator::OnEntryShown(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kGlic);
  state_observers_.Notify(&StateObserver::VisibilityChanged, true);
}

void GlicSidePanelCoordinator::OnGlicEnabledChanged() {
  bool is_allowed = glic::GlicEnabling::IsEnabledForProfile(
      tab_->GetBrowserWindowInterface()->GetProfile());

  // Active tab sets visibility of toolbar action.
  // TODO: Consider moving this responsibility to a browser level singleton
  if (tab_->IsActivated()) {
    glic_action_->SetVisible(is_allowed);
  }
  // Register / deregister side panel entry.
  if (is_allowed) {
    CreateAndRegisterEntry();
  } else {
    SidePanelEntry::Key glic_key =
        SidePanelEntry::Key(SidePanelEntry::Id::kGlic);
    if (side_panel_coordinator_->IsSidePanelEntryShowing(glic_key)) {
      side_panel_coordinator_->Close();
    }
    SidePanelEntry* glic_entry = side_panel_registry_->GetEntryForKey(glic_key);
    if (glic_entry) {
      glic_entry->RemoveObserver(this);
    }
    side_panel_registry_->Deregister(glic_key);
  }
}

std::unique_ptr<views::View> GlicSidePanelCoordinator::CreateView(
    SidePanelEntryScope& scope) {
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      tab_->GetBrowserWindowInterface()->GetProfile());
  if (!glic_service) {
    return nullptr;
  }
  return glic_service->window_controller().CreateViewForSidePanel(
      scope.GetTabInterface());
}

void GlicSidePanelCoordinator::AddObserver(StateObserver* observer) {
  state_observers_.AddObserver(observer);
}

void GlicSidePanelCoordinator::RemoveObserver(StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

}  // namespace glic
