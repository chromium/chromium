// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/ui/web_applications/web_app_uninstall_dialog.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class NativeWindowTracker;
class Profile;
class WebAppUninstallDialogViews;

namespace views {
class Checkbox;
}

// The dialog's view, owned by the views framework.
class WebAppUninstallDialogDelegateView : public views::DialogDelegateView {
 public:
  // Constructor for view component of dialog.
  WebAppUninstallDialogDelegateView(
      Profile* profile,
      WebAppUninstallDialogViews* dialog_view,
      web_app::AppId app_id,
      std::map<SquareSizePx, SkBitmap> icon_bitmaps);
  WebAppUninstallDialogDelegateView(const WebAppUninstallDialogDelegateView&) =
      delete;
  WebAppUninstallDialogDelegateView& operator=(
      const WebAppUninstallDialogDelegateView&) = delete;
  ~WebAppUninstallDialogDelegateView() override;

  void ProcessAutoConfirmValue();

 private:
  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  gfx::ImageSkia GetWindowIcon() override;

  // Uninstalls the web app. Returns true on success.
  bool Uninstall();
  void ClearWebAppSiteData();

  void OnDialogAccepted();
  void OnDialogCanceled();

  WebAppUninstallDialogViews* dialog_;

  views::Checkbox* checkbox_ = nullptr;
  gfx::ImageSkia image_;

  // The web app we are showing the dialog for.
  const web_app::AppId app_id_;
  // The dialog needs start_url copy even if app gets uninstalled.
  GURL app_start_url_;

  Profile* const profile_;
};

// The implementation of the uninstall dialog for web apps.
class WebAppUninstallDialogViews : public web_app::WebAppUninstallDialog,
                                   public web_app::AppRegistrarObserver {
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
                        OnWebAppUninstallDialogClosed closed_callback) override;
  void SetDialogShownCallbackForTesting(base::OnceClosure callback) override;

  void UninstallStarted();
  void CallCallback(bool uninstalled);

 private:
  // web_app::AppRegistrarObserver:
  void OnWebAppUninstalled(const web_app::AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  void OnIconsRead(std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  // The dialog's parent window.
  const gfx::NativeWindow parent_;

  // The callback we will call Accepted/Canceled on after confirmation dialog.
  OnWebAppUninstallDialogClosed closed_callback_;

  base::OnceClosure dialog_shown_callback_for_testing_;

  // Tracks whether |parent_| got destroyed.
  std::unique_ptr<NativeWindowTracker> parent_window_tracker_;

  ScopedObserver<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      registrar_observer_{this};

  WebAppUninstallDialogDelegateView* view_ = nullptr;

  // The web app we are showing the dialog for.
  web_app::AppId app_id_;
  Profile* const profile_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<WebAppUninstallDialogViews> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_
