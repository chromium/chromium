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
    const gfx::ImageSkia& icon_image,
    const std::u16string& app_name,
    const GURL& start_url,
    bool is_maskable,
    InstallOsType os_type,
    InstallDialogType install_type,
    base::RepeatingCallback<void(const std::u16string&)>
        text_tracker_callback) {
  os_type_ = os_type;
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // kInstallDialog
  auto* install_dialog_view = AddChildView(WebAppInstallIntroView::Create(
      install_type, icon_image, app_name, start_url, is_maskable,
      std::move(text_tracker_callback)));
  install_step_to_view_[InstallDialogStep::kInstallDialog] =
      install_dialog_view;

  // kInstallerOptions
  auto* options = CreateInstallOptionsView();

  options->SetVisible(false);
  install_step_to_view_[InstallDialogStep::kInstallerOptions] = options;

  // kProgress
  auto* progress = AddChildView(
      views::Builder<views::Label>().SetText(u"Progress View").Build());
  progress->SetVisible(false);
  install_step_to_view_[InstallDialogStep::kProgress] = progress;

  // kSuccessful launch app button
  auto* successful = AddChildView(
      views::Builder<views::Label>().SetText(u"Successful View").Build());
  successful->SetVisible(false);
  install_step_to_view_[InstallDialogStep::kSuccessful] = successful;
}

WebAppInstallFlowView::~WebAppInstallFlowView() = default;

// Creates the installer options view based on the os_type_.
views::View* WebAppInstallFlowView::CreateInstallOptionsView() {
  std::u16string label;
  switch (os_type_) {
    case InstallOsType::kMac:
      label = u"Installer options Mac view";
      break;
    case InstallOsType::kWin:
      label = u"Installer options Windows view";
      break;
    case InstallOsType::kCros:
      label = u"Installer options ChromeOS view";
      break;
    default:
      label = u"Installer options Other view";
  }
  return AddChildView(views::Builder<views::Label>().SetText(label).Build());
}

void WebAppInstallFlowView::UpdateStepVisibility(
    InstallDialogStep current_step) {
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
