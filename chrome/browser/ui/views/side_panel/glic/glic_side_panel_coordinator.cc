// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"

#include "base/functional/callback.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"

namespace glic {

GlicSidePanelCoordinator::GlicSidePanelCoordinator(Profile* profile)
    : glic_service_(GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {}

void GlicSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic),
      base::BindRepeating(&GlicSidePanelCoordinator::CreateGlicWebView,
                          base::Unretained(this)),
      /*default_content_width_callback=*/base::NullCallback());
  entry->AddObserver(this);
  global_registry->Register(std::move(entry));
}

void GlicSidePanelCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  if (glic_service_) {
    glic_service_->ClosePanel();
  }
}

std::unique_ptr<views::View> GlicSidePanelCoordinator::CreateGlicWebView(
    SidePanelEntryScope& scope) {
  if (!glic_service_) {
    return nullptr;
  }
  return glic_service_->window_controller().CreateGlicViewForSidePanel();
}

}  // namespace glic
