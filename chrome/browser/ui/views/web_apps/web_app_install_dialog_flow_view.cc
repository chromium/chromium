// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_flow_view.h"

#include <memory>

#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace web_app {

// A generic view that represents the installation flow.
WebAppInstallFlowView::WebAppInstallFlowView(const gfx::ImageSkia& icon_image,
                                             const std::u16string& app_name,
                                             const GURL& start_url,
                                             bool is_maskable) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // kInstallDialog
  auto* install_dialog_view = AddChildView(WebAppIconNameAndOriginView::Create(
      icon_image, app_name, start_url, is_maskable));
  install_step_to_view_[InstallDialogStep::kInstallDialog] =
      install_dialog_view;

  // kInstallerOptions
  auto* options =
      AddChildView(std::make_unique<views::Label>(u"Installer Options View"));
  options->SetVisible(false);
  install_step_to_view_[InstallDialogStep::kInstallerOptions] = options;

  // kProgress
  auto* progress =
      AddChildView(std::make_unique<views::Label>(u"Progress View"));
  progress->SetVisible(false);
  install_step_to_view_[InstallDialogStep::kProgress] = progress;

  // kSuccessful launch app button
  auto* successful =
      AddChildView(std::make_unique<views::Label>(u"Successful View"));
  successful->SetVisible(false);
  install_step_to_view_[InstallDialogStep::kSuccessful] = successful;
}

WebAppInstallFlowView::~WebAppInstallFlowView() = default;

// Assigns a view to the provided InstallDialogStep in the
// WebAppInstallFlowView.
void WebAppInstallFlowView::SetStepView(InstallDialogStep step,
                                        std::unique_ptr<views::View> view) {
  auto pair = install_step_to_view_.find(step);
  bool was_visible = true;
  if (pair != install_step_to_view_.end()) {
    views::View* old_view = pair->second;
    was_visible = old_view->GetVisible();
    pair->second = nullptr;
    RemoveChildViewT(old_view);
  }
  view->SetVisible(was_visible);
  install_step_to_view_[step] = AddChildView(std::move(view));
}

// Ensures visibility is appropriately updated for each views.
void WebAppInstallFlowView::UpdateStepVisibility(
    InstallDialogStep current_step) {
  for (auto const& [step, view] : install_step_to_view_) {
    if (view) {
      view->SetVisible(step == current_step);
    }
  }
  PreferredSizeChanged();
}

base::WeakPtr<WebAppInstallFlowView> WebAppInstallFlowView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace web_app
