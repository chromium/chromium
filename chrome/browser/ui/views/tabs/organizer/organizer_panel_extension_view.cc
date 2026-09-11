// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_extension_view.h"

#include <utility>

#include "base/check_op.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "ui/views/view_class_properties.h"

OrganizerPanelExtensionView::OrganizerPanelExtensionView(
    BrowserWindowInterface& browser)
    : OrganizerPanelView(browser),
      browser_(browser),
      root_action_item_(BrowserActions::From(&browser)->root_action_item()),
      state_controller_subscription_(
          OrganizerPanelController::From(&browser)->RegisterOnStateChanged(
              base::BindRepeating(
                  &OrganizerPanelExtensionView::OnOrganizerPanelStateChanged,
                  base::Unretained(this)))) {}

OrganizerPanelExtensionView::~OrganizerPanelExtensionView() {
  ResetExtensionContent();
}

bool OrganizerPanelExtensionView::IsInExtensionModeForTesting() const {
  return true;
}

void OrganizerPanelExtensionView::ResetExtensionContent() {
  if (current_extension_id_ && extension_host_ && browser_->GetProfile() &&
      !browser_->IsDeleteScheduled()) {
    if (auto* const service =
            extensions::SidePanelService::Get(browser_->GetProfile())) {
      service->DispatchOnClosedEvent(
          *current_extension_id_,
          extensions::ExtensionTabUtil::GetWindowId(&*browser_),
          /*tab_id=*/std::nullopt, extension_host_->initial_url().GetPath());
    }
  }

  scoped_view_observation_.Reset();
  scoped_host_observation_.Reset();
  if (extension_view_) {
    RemoveChildViewT(std::exchange(extension_view_, nullptr));
  }
  extension_host_.reset();
  current_extension_id_.reset();
}

void OrganizerPanelExtensionView::HandleCloseExtensionHost(
    extensions::ExtensionHost* host) {
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionToggleOrganizerPanel, root_action_item_);
  if (action_item) {
    action_item->InvokeAction();
  }
}

void OrganizerPanelExtensionView::UpdateExtensionContent(
    const extensions::ExtensionId& extension_id) {
  if (current_extension_id_ == extension_id && extension_view_ &&
      extension_host_) {
    return;
  }

  ResetExtensionContent();

  if (!browser_->GetProfile()) {
    return;
  }

  Profile* const profile = browser_->GetProfile();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          extension_id);
  if (!extension) {
    return;
  }

  extensions::SidePanelService* service =
      extensions::SidePanelService::Get(profile);
  if (!service) {
    return;
  }

  auto options = service->GetOptions(*extension, /*tab_id=*/std::nullopt);
  if (!options.enabled.value_or(false) || !options.path.has_value()) {
    return;
  }

  GURL side_panel_url = GURL(*options.path);
  if (!side_panel_url.SchemeIsHTTPOrHTTPS()) {
    side_panel_url = extension->ResolveExtensionURL(*options.path);
  }

  extension_host_ = extensions::ExtensionViewHostFactory::CreateSidePanelHost(
      *extension, side_panel_url, &*browser_, /*tab_interface=*/nullptr);
  if (!extension_host_) {
    return;
  }

  // Handle window.close() inside extension side panel.
  extension_host_->SetCloseHandler(
      base::BindOnce(&OrganizerPanelExtensionView::HandleCloseExtensionHost,
                     base::Unretained(this)));

  auto extension_view =
      std::make_unique<ExtensionViewViews>(profile, extension_host_.get());
  extension_view->Init();
  extension_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  extension_view->SetProperty(views::kElementIdentifierKey, kWebViewElementId);

  extension_view_ = AddChildView(std::move(extension_view));
  current_extension_id_ = extension_id;

  scoped_view_observation_.Reset();
  scoped_view_observation_.Observe(extension_view_);
  scoped_host_observation_.Reset();
  scoped_host_observation_.Observe(extension_host_.get());

  service->DispatchOnOpenedEvent(
      extension_id, extensions::ExtensionTabUtil::GetWindowId(&*browser_),
      /*tab_id=*/std::nullopt, side_panel_url.GetPath());
}

void OrganizerPanelExtensionView::UpdateDefaultExtensionContent() {
  if (!browser_->GetProfile()) {
    return;
  }

  auto* registry = extensions::ExtensionRegistry::Get(browser_->GetProfile());
  auto* service = extensions::SidePanelService::Get(browser_->GetProfile());
  if (!registry || !service) {
    return;
  }

  for (const auto& extension : registry->enabled_extensions()) {
    auto options = service->GetOptions(*extension, std::nullopt);
    if (options.enabled.value_or(false) && options.path.has_value()) {
      UpdateExtensionContent(extension->id());
      return;
    }
  }
}

void OrganizerPanelExtensionView::OnViewDestroying() {
  // Clear `extension_view_` so ResetExtensionContent() will not attempt to
  // remove or delete a view that is already being destroyed.
  extension_view_ = nullptr;
  ResetExtensionContent();
}

void OrganizerPanelExtensionView::OnExtensionHostDestroyed(
    extensions::ExtensionHost* host) {
  DCHECK_EQ(extension_host_.get(), host);
  scoped_host_observation_.Reset();
  if (current_extension_id_ && browser_->GetProfile()) {
    auto* service = extensions::SidePanelService::Get(browser_->GetProfile());
    if (service) {
      service->DispatchOnClosedEvent(
          *current_extension_id_,
          extensions::ExtensionTabUtil::GetWindowId(&*browser_),
          /*tab_id=*/std::nullopt, host->initial_url().GetPath());
    }
  }
  // Release ownership because `host` is currently executing its destructor.
  extension_host_.release();
  ResetExtensionContent();
}

void OrganizerPanelExtensionView::OnOrganizerPanelStateChanged(
    OrganizerPanelController* state_controller) {
  if (state_controller->IsOrganizerPanelVisible() &&
      organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled()) {
    if (state_controller->active_extension_id().has_value()) {
      UpdateExtensionContent(*state_controller->active_extension_id());
    } else {
      UpdateDefaultExtensionContent();
    }
  }
}
