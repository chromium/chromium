// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_FLOW_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_FLOW_DIALOG_DELEGATE_H_

#include <iosfwd>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "ui/base/identifier/unique_identifier.h"

namespace content {
class WebContents;
}

namespace webapps {
class MlInstallOperationTracker;
}

namespace web_app {

class WebAppScreenshotFetcher;
class WebAppInstallFlowView;
struct WebAppInstallInfo;

enum class InstallDialogStep {
  kInstallDialog = 0,
  kInstallerOptions = 1,
  kProgress = 2,
  kSuccessful = 3,
};

enum class InstallOsType { kMac, kWin, kCros, kOther };
std::ostream& operator<<(std::ostream& os, InstallOsType type);

class WebAppInstallFlowDialogDelegate : public WebAppInstallDialogDelegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInstallDialogFlowViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLearnMoreButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonId);

  WebAppInstallFlowDialogDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> install_info,
      std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
      AppInstallationAcceptanceCallback callback,
      PwaInProductHelpState iph_state,
      PrefService* prefs,
      feature_engagement::Tracker* tracker,
      InstallDialogType dialog_type,
      InstallOsType os_type);

  ~WebAppInstallFlowDialogDelegate() override;

  static void Show(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> install_info,
      std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
      AppInstallationAcceptanceCallback callback,
      PwaInProductHelpState iph_state,
      base::WeakPtr<WebAppScreenshotFetcher> screenshot_fetcher,
      bool show_initiating_origin,
      InstallDialogType dialog_type,
      InstallOsType os_type);

  void SetFlowView(base::WeakPtr<WebAppInstallFlowView> flow_view) {
    flow_view_ = std::move(flow_view);
  }

  bool OnOkButtonClicked() override;

  base::WeakPtr<WebAppInstallFlowDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  InstallDialogStep current_step_ = InstallDialogStep::kInstallDialog;
  InstallOsType os_type_;
  base::WeakPtr<WebAppInstallFlowView> flow_view_;

 private:
  void OnLearnMoreButtonClicked();
  void UpdateDialogTitle(InstallDialogStep step);
  base::WeakPtrFactory<WebAppInstallFlowDialogDelegate> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_FLOW_DIALOG_DELEGATE_H_
