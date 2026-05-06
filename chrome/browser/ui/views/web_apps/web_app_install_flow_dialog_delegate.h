// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_FLOW_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_FLOW_DIALOG_DELEGATE_H_

#include <iosfwd>
#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "ui/base/identifier/unique_identifier.h"

namespace content {
class WebContents;
}

namespace webapps {
class MlInstallOperationTracker;
}

namespace web_app {

class ProgressDelay;
class WebAppScreenshotFetcher;
class WebAppInstallFlowView;
class WebAppInstallProgressView;
class WebAppInstallOptionsView;
struct WebAppInstallInfo;

enum class InstallDialogStep {
  kInstallDialog = 0,
  kInstallerOptions = 1,
  kProgress = 2,
  kSuccessful = 3,
};

enum class InstallOsType { kMac, kWin, kCros, kOther };
inline constexpr int kLargeImageSize = 80;
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
      WebAppInstallationAcceptanceCallback callback,
      PwaInProductHelpState iph_state,
      PrefService* prefs,
      feature_engagement::Tracker* tracker,
      InstallDialogType dialog_type,
      InstallOsType os_type,
      std::unique_ptr<ProgressDelay> progress_delay);

  ~WebAppInstallFlowDialogDelegate() override;

  static void Show(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> install_info,
      std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
      WebAppInstallationAcceptanceCallback callback,
      PwaInProductHelpState iph_state,
      base::WeakPtr<WebAppScreenshotFetcher> screenshot_fetcher,
      bool show_initiating_origin,
      InstallDialogType dialog_type,
      InstallOsType os_type,
      std::unique_ptr<ProgressDelay> progress_delay);

  void SetFlowView(base::WeakPtr<WebAppInstallFlowView> flow_view) {
    flow_view_ = std::move(flow_view);
  }

  void SetProgressView(base::WeakPtr<WebAppInstallProgressView> progress_view) {
    progress_view_ = std::move(progress_view);
  }

  bool AdvanceToNextStepOrClose();

  void OnAccept() override;
  void OnProgress(std::optional<double> percent);

  base::WeakPtr<WebAppInstallFlowDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  InstallDialogStep current_step_ = InstallDialogStep::kInstallDialog;
  InstallOsType os_type_;
  base::WeakPtr<WebAppInstallFlowView> flow_view_;
  base::WeakPtr<WebAppInstallProgressView> progress_view_;
  base::WeakPtr<WebAppInstallOptionsView> options_view_;

 private:
  void OnLearnMoreButtonClicked();
  void UpdateDialogTitleAndHeader(InstallDialogStep step);
  void UpdateProgressAndMaybeAdvance();
  void OnInstallResult(bool success, base::OnceClosure reparent_closure);
  void OnAcceptCallback(bool success,
                        std::unique_ptr<WebAppInstallInfo> web_app_info);

  WebAppInstallationAcceptanceCallback callback_;
  std::unique_ptr<ProgressDelay> progress_delay_;
  bool install_success_ = false;
  std::optional<double> timer_percentage_ = 0.0;
  base::OnceClosure reparent_closure_;
  base::WeakPtrFactory<WebAppInstallFlowDialogDelegate> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_FLOW_DIALOG_DELEGATE_H_
