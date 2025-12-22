// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator_impl.h"

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
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
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/actions.h"
#include "ui/compositor/layer.h"
#include "ui/views/layout/fill_layout.h"

namespace glic {
GlicSidePanelCoordinatorImpl::GlicSidePanelCoordinatorImpl(
    tabs::TabInterface* tab,
    SidePanelRegistry* side_panel_registry)
    : GlicSidePanelCoordinator(tab),
      tab_(tab),
      side_panel_registry_(side_panel_registry) {
  CHECK(GlicEnabling::IsMultiInstanceEnabled());
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      tab->GetBrowserWindowInterface()->GetProfile());
  on_glic_enabled_changed_subscription_ =
      glic_service->enabling().RegisterAllowedChanged(base::BindRepeating(
          &GlicSidePanelCoordinatorImpl::OnGlicEnabledChanged,
          base::Unretained(this)));
  if (glic_service->enabling().IsAllowed()) {
    CreateAndRegisterEntry();
  }
}

GlicSidePanelCoordinatorImpl::~GlicSidePanelCoordinatorImpl() {
  if (entry_) {
    entry_->RemoveObserver(this);
  }
}

void GlicSidePanelCoordinatorImpl::CreateAndRegisterEntry() {
  if (side_panel_registry_->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kGlic))) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      base::FeatureList::IsEnabled(features::kGlicUseToolbarHeightSidePanel)
          ? SidePanelEntry::PanelType::kToolbar
          : SidePanelEntry::PanelType::kContent,
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic),
      base::BindRepeating(&GlicSidePanelCoordinatorImpl::CreateView,
                          base::Unretained(this)),
      base::BindRepeating(&GlicSidePanelCoordinatorImpl::GetPreferredWidth,
                          base::Unretained(this)));
  entry->set_should_show_header(false);
  entry->set_should_show_outline(false);
  entry->set_should_show_ephemerally_in_toolbar(false);
  entry->AddObserver(this);
  entry_ = entry->GetWeakPtr();
  side_panel_registry_->Register(std::move(entry));
}

void GlicSidePanelCoordinatorImpl::Show(bool suppress_animations) {
  auto* window_side_panel_coordinator = GetWindowSidePanelCoordinator();
  if (!window_side_panel_coordinator || !entry_) {
    return;
  }
  if (!tab_->IsActivated()) {
    if (entry_) {
      // The tab is in the background, so we just mark it for showing the glic
      // side panel when it becomes the active tab. eg. This flow can be
      // encountered when a background tab is bound via daisy chaining.
      side_panel_registry_->SetActiveEntry(entry_.get());
    }
    return;
  }
  SidePanelUIBase::UniqueKey unique_key{
      .tab_handle = tab_->GetHandle(),
      .key = SidePanelEntry::Key(SidePanelEntry::Id::kGlic)};
  window_side_panel_coordinator->Show(unique_key, std::nullopt,
                                      suppress_animations);
}

void GlicSidePanelCoordinatorImpl::Close() {
  auto* window_side_panel_coordinator = GetWindowSidePanelCoordinator();
  if (!window_side_panel_coordinator || !entry_) {
    return;
  }
  if (IsShowing()) {
    window_side_panel_coordinator->Close(entry_->type());
    return;
  }
  if (state_ == State::kBackgrounded) {
    CHECK(IsGlicSidePanelActive());
    side_panel_registry_->ResetActiveEntryFor(entry_->type());
    SetState(State::kClosed);
  }
}

bool GlicSidePanelCoordinatorImpl::IsShowing() const {
  return state_ == State::kShown;
}

GlicSidePanelCoordinator::State GlicSidePanelCoordinatorImpl::state() {
  return state_;
}

void GlicSidePanelCoordinatorImpl::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kGlic);
  pending_hide_reason_ = reason;
}

void GlicSidePanelCoordinatorImpl::OnEntryHideCancelled(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kGlic);
  pending_hide_reason_.reset();
}

void GlicSidePanelCoordinatorImpl::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kGlic);
  CHECK(pending_hide_reason_.has_value());
  if (pending_hide_reason_ == SidePanelEntryHideReason::kBackgrounded) {
    SetState(State::kBackgrounded);
  } else {
    SetState(State::kClosed);
  }
}

void GlicSidePanelCoordinatorImpl::OnEntryShown(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kGlic);
  SetState(State::kShown);
}

void GlicSidePanelCoordinatorImpl::OnGlicEnabledChanged() {
  // Maybe register side panel entry if not yet registered.
  if (glic::GlicEnabling::IsEnabledForProfile(
          tab_->GetBrowserWindowInterface()->GetProfile())) {
    CreateAndRegisterEntry();
  }
}

std::unique_ptr<views::View> GlicSidePanelCoordinatorImpl::CreateView(
    SidePanelEntryScope& scope) {
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      tab_->GetBrowserWindowInterface()->GetProfile());
  if (!glic_service) {
    return nullptr;
  }
  // Provide the side panel with an empty container View so that different
  // `GlicUiEmbedder`s can update its contents as needed.
  auto glic_container = std::make_unique<views::View>();
  if (base::FeatureList::IsEnabled(features::kGlicUseToolbarHeightSidePanel)) {
    glic_container->SetPaintToLayer();
    glic_container->layer()->SetFillsBoundsOpaquely(false);
  }
  glic_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  glic_container_tracker_.SetView(glic_container.get());

  if (contents_view_) {
    glic_container->AddChildView(std::move(contents_view_));
  }

  return glic_container;
}

base::CallbackListSubscription GlicSidePanelCoordinatorImpl::AddStateCallback(
    base::RepeatingCallback<void(State state)> callback) {
  return state_changed_callbacks_.Add(std::move(callback));
}

void GlicSidePanelCoordinatorImpl::SetContentsView(
    std::unique_ptr<views::View> contents_view) {
  if (!glic_container_tracker_) {
    contents_view_ = std::move(contents_view);
    return;
  }

  glic_container_tracker_.view()->RemoveAllChildViews();
  glic_container_tracker_.view()->AddChildView(std::move(contents_view));
}

int GlicSidePanelCoordinatorImpl::GetPreferredWidth() {
  return features::kGlicSidePanelMinWidth.Get();
}

bool GlicSidePanelCoordinatorImpl::IsGlicSidePanelActive() {
  if (!side_panel_registry_) {
    return false;
  }
  auto* glic_side_panel_entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntryKey(SidePanelEntry::Id::kGlic));
  if (!glic_side_panel_entry) {
    return false;
  }
  const auto& active_entry =
      side_panel_registry_->GetActiveEntryFor(glic_side_panel_entry->type());
  if (!active_entry.has_value() ||
      active_entry.value() != glic_side_panel_entry) {
    return false;
  }
  return true;
}

SidePanelCoordinator*
GlicSidePanelCoordinatorImpl::GetWindowSidePanelCoordinator() const {
  if (auto* window = tab_->GetBrowserWindowInterface()) {
    return SidePanelCoordinator::From(window);
  }
  return nullptr;
}

void GlicSidePanelCoordinatorImpl::SetState(State new_state) {
  state_ = new_state;
  state_changed_callbacks_.Notify(state_);
}

}  // namespace glic
