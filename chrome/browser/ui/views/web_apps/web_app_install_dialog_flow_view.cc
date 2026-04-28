// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_flow_view.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_intro_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace web_app {

// A generic view that represents the installation flow.
WebAppInstallFlowView::WebAppInstallFlowView(
    absl::flat_hash_map<InstallDialogStep, std::unique_ptr<views::View>>
        install_step_to_view) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  for (auto& [step, view] : install_step_to_view) {
    auto* raw_view = AddChildView(std::move(view));
    install_step_to_view_[step] = raw_view;
    // Set all to invisible except kInstallDialog
    raw_view->SetVisible(step == InstallDialogStep::kInstallDialog);
  }
}

WebAppInstallFlowView::~WebAppInstallFlowView() = default;

void WebAppInstallFlowView::UpdateStepVisibility(
    InstallDialogStep current_step) {
  current_step_for_testing_ = current_step;
  for (auto const& [step, view] : install_step_to_view_) {
    if (view) {
      view->SetVisible(step == current_step);
    }
  }
  PreferredSizeChanged();
}

views::View* WebAppInstallFlowView::GetViewForStep(InstallDialogStep step) {
  auto it = install_step_to_view_.find(step);
  return it != install_step_to_view_.end() ? it->second.get() : nullptr;
}

base::WeakPtr<WebAppInstallFlowView> WebAppInstallFlowView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace web_app
