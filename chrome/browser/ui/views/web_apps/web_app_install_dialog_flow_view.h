// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_

#include <map>

#include "base/functional/callback_forward.h"
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
                        bool is_maskable,
                        InstallOsType os_type,
                        InstallDialogType install_type,
                        base::RepeatingCallback<void(const std::u16string&)>
                            text_tracker_callback);
  ~WebAppInstallFlowView() override;

  base::WeakPtr<WebAppInstallFlowView> GetWeakPtr();

  // Shows the view for the given step, and hides all others.
  void UpdateStepVisibility(InstallDialogStep current_step);

  views::View* GetViewForStep(InstallDialogStep step);

 private:
  views::View* CreateInstallOptionsView();

  std::map<InstallDialogStep, raw_ptr<views::View>> install_step_to_view_;
  InstallOsType os_type_;

  base::WeakPtrFactory<WebAppInstallFlowView> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_FLOW_VIEW_H_
