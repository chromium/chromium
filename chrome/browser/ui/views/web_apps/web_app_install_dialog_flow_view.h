// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/views/view.h"

namespace web_app {

// A simple view that containerizes multiple steps of the web app installation
// flow. It accepts a map of steps to views, and its only job is to show one
// view at a time, controlled by `UpdateStepVisibility`. By default, the
// `InstallDialogStep::kInstallDialog` step is shown and the others are hidden.
class WebAppInstallFlowView : public views::View {
 public:
  explicit WebAppInstallFlowView(
      absl::flat_hash_map<InstallDialogStep, std::unique_ptr<views::View>>
          install_step_to_view);
  ~WebAppInstallFlowView() override;

  base::WeakPtr<WebAppInstallFlowView> GetWeakPtr();

  // Shows the view for the given step, and hides all others.
  void UpdateStepVisibility(InstallDialogStep current_step);

  views::View* GetViewForStep(InstallDialogStep step);

 private:
  absl::flat_hash_map<InstallDialogStep, raw_ptr<views::View>>
      install_step_to_view_;

  base::WeakPtrFactory<WebAppInstallFlowView> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_
