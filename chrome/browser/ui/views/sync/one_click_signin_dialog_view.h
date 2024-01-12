// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_DIALOG_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sync/one_click_signin_links_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/dialog_delegate.h"

// This class allows users to confirm sync signin in cases where signin is
// untrusted.
class OneClickSigninDialogView : public views::DialogDelegateView {
  METADATA_HEADER(OneClickSigninDialogView, views::DialogDelegateView)

 public:
  OneClickSigninDialogView(const OneClickSigninDialogView&) = delete;
  OneClickSigninDialogView& operator=(const OneClickSigninDialogView&) = delete;
  ~OneClickSigninDialogView() override;

  // Show the one-click signin dialog if not already showing.
  static void ShowDialog(const std::u16string& email,
                         std::unique_ptr<OneClickSigninLinksDelegate> delegate,
                         gfx::NativeWindow window,
                         base::OnceCallback<void(bool)> confirmed_callback);

  static bool IsShowing();

  static void Hide();

  // Gets the global dialog view. If its not showing returns nullptr. This
  // method is meant to be called only from tests.
  static OneClickSigninDialogView* view_for_testing() { return dialog_view_; }

  // Overridden from views::DialogDelegateView:
  void WindowClosing() override;
  bool Accept() override;

 protected:
  // Creates a OneClickSigninDialogView.
  OneClickSigninDialogView(
      const std::u16string& email,
      std::unique_ptr<OneClickSigninLinksDelegate> delegate,
      base::OnceCallback<void(bool)> confirmed_callback);

 private:
  friend class OneClickSigninDialogViewTest;

  // The user's email address to be used for sync.
  const std::u16string email_;

  // This callback is nulled once its called, so that it is called only once.
  // It will be called when the bubble is closed if it has not been called
  // and nulled earlier.
  base::OnceCallback<void(bool)> confirmed_callback_;

  // The bubble, if we're showing one.
  static OneClickSigninDialogView* dialog_view_;
};

BEGIN_VIEW_BUILDER(, OneClickSigninDialogView, views::DialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, OneClickSigninDialogView)

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_DIALOG_VIEW_H_
