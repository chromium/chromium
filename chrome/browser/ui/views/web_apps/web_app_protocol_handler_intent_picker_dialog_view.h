// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class Profile;

namespace views {
class Checkbox;
class ImageView;
}  // namespace views

// This class extends DialogDelegateView and needs to be owned
// by the views framework.
class WebAppProtocolHandlerIntentPickerView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(WebAppProtocolHandlerIntentPickerView);

  WebAppProtocolHandlerIntentPickerView(
      const GURL& url,
      Profile* profile,
      const web_app::AppId& app_id,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      chrome::WebAppProtocolHandlerAcceptanceCallback close_callback);

  WebAppProtocolHandlerIntentPickerView(
      const WebAppProtocolHandlerIntentPickerView&) = delete;
  WebAppProtocolHandlerIntentPickerView& operator=(
      const WebAppProtocolHandlerIntentPickerView&) = delete;
  ~WebAppProtocolHandlerIntentPickerView() override;

  static void Show(
      const GURL& url,
      Profile* profile,
      const web_app::AppId& app_id,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      chrome::WebAppProtocolHandlerAcceptanceCallback close_callback);

  static void SetDefaultRememberSelectionForTesting(bool remember_state);

 private:
  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  const web_app::AppId& GetSelectedAppId() const;
  void OnAccepted();
  void OnCanceled();
  void OnClosed();
  void InitChildViews();
  void OnIconsRead(std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  // Runs the close_callback_ provided during Show() if it exists.
  void RunCloseCallback(bool allowed, bool remember_user_choice);

  const GURL url_;
  Profile* const profile_;
  const web_app::AppId app_id_;
  views::Checkbox* remember_selection_checkbox_;
  views::ImageView* icon_image_view_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  chrome::WebAppProtocolHandlerAcceptanceCallback close_callback_;
  base::WeakPtrFactory<WebAppProtocolHandlerIntentPickerView> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_
