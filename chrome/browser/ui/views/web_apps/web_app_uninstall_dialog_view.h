// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/ui/web_applications/web_app_uninstall_dialog.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class Profile;
class WebAppUninstallDialogViews;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}

namespace views {
class Checkbox;
class NativeWindowTracker;
}

// The dialog's view, owned by the views framework.
class WebAppUninstallDialogDelegateView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(WebAppUninstallDialogDelegateView);
  // Constructor for view component of dialog.
  WebAppUninstallDialogDelegateView(
      Profile* profile,
      WebAppUninstallDialogViews* dialog_view,
      web_app::AppId app_id,
      webapps::WebappUninstallSource uninstall_source,
      std::map<SquareSizePx, SkBitmap> icon_bitmaps);
  WebAppUninstallDialogDelegateView(const WebAppUninstallDialogDelegateView&) =
      delete;
  WebAppUninstallDialogDelegateView& operator=(
      const WebAppUninstallDialogDelegateView&) = delete;
  ~WebAppUninstallDialogDelegateView() override;

  void ProcessAutoConfirmValue();

 private:
  // views::DialogDelegateView:
  ui::ImageModel GetWindowIcon() override;

  // Uninstalls the web app.
  void Uninstall();
  void ClearWebAppSiteData();

  void OnDialogAccepted();
  void OnDialogCanceled();

  raw_ptr<WebAppUninstallDialogViews, DanglingUntriaged> dialog_;

  raw_ptr<views::Checkbox> checkbox_ = nullptr;
  gfx::ImageSkia image_;

  // The web app we are showing the dialog for.
  const web_app::AppId app_id_;

  // The dialog needs start_url copy even if app gets uninstalled.
  GURL app_start_url_;

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  webapps::WebappUninstallSource uninstall_source_;
};

// The implementation of the uninstall dialog for web apps.
class WebAppUninstallDialogViews
    : public web_app::WebAppUninstallDialog,
      public web_app::WebAppInstallManagerObserver {
 public:
  // Implement this callback to handle checking for the dialog's header message.
  using OnWillShowCallback =
      base::RepeatingCallback<void(WebAppUninstallDialogViews*)>;

  WebAppUninstallDialogViews(Profile* profile, gfx::NativeWindow parent);
  WebAppUninstallDialogViews(const WebAppUninstallDialogViews&) = delete;
  WebAppUninstallDialogViews& operator=(const WebAppUninstallDialogViews&) =
      delete;
  ~WebAppUninstallDialogViews() override;

  // web_app::WebAppUninstallDialog:
  void ConfirmUninstall(const web_app::AppId& app_id,
                        webapps::WebappUninstallSource uninstall_source,
                        OnWebAppUninstallDialogClosed closed_callback) override;
  void SetDialogShownCallbackForTesting(base::OnceClosure callback) override;

  // The following methods are used by WebAppUninstallDialogDelegateView to
  // report the uninstallation request status. After calling one of these
  // methods, it is invalid to call any of them again.

  // Called when the view is triggering an uninstallation with the
  // WebAppProvider system. Returns a callback to be passed to this system.
  base::OnceCallback<void(webapps::UninstallResultCode code)>
  UninstallStarted();

  // Called to signify that the uninstall has been cancelled.
  void UninstallCancelled();

 private:
  // web_app::WebAppInstallManagerObserver:
  void OnWebAppWillBeUninstalled(const web_app::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  void OnIconsRead(webapps::WebappUninstallSource uninstall_source,
                   std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  // The dialog's parent window.
  const gfx::NativeWindow parent_;

  // The callback we will call Accepted/Canceled on after confirmation dialog.
  OnWebAppUninstallDialogClosed closed_callback_;

  base::OnceClosure dialog_shown_callback_for_testing_;

  // Tracks whether |parent_| got destroyed.
  std::unique_ptr<views::NativeWindowTracker> parent_window_tracker_;

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  raw_ptr<WebAppUninstallDialogDelegateView> view_ = nullptr;

  // The web app we are showing the dialog for.
  web_app::AppId app_id_;
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<WebAppUninstallDialogViews> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_
