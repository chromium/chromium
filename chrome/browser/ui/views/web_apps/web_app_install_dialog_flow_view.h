// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "ui/views/view.h"

namespace web_app {

// A generic view that represents the flow of the web app installation.
// It contains different views for each step of the installation process.
class WebAppInstallFlowView : public views::View {
 public:
  WebAppInstallFlowView(const gfx::ImageSkia& icon_image,
                        const std::u16string& app_name,
                        const GURL& start_url,
                        bool is_maskable);
  ~WebAppInstallFlowView() override;

  base::WeakPtr<WebAppInstallFlowView> GetWeakPtr();

  void SetStepView(InstallDialogStep step, std::unique_ptr<views::View> view);

  void UpdateStepVisibility(InstallDialogStep current_step);

 private:
  std::map<InstallDialogStep, raw_ptr<views::View>> install_step_to_view_;

  base::WeakPtrFactory<WebAppInstallFlowView> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_
