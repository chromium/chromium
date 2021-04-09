// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/command_line.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class Profile;
class WebAppHoverButton;

namespace views {
class Checkbox;
class ScrollView;
}  // namespace views

// This class extends DialogDelegateView and needs to be owned
// by the views framework.
class WebAppProtocolHandlerIntentPickerView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(WebAppProtocolHandlerIntentPickerView);

  WebAppProtocolHandlerIntentPickerView(
      const GURL& url,
      Profile* profile,
      const base::CommandLine& command_line,
      std::vector<std::string> app_ids,
      base::OnceCallback<void(bool accepted)> close_callback);

  WebAppProtocolHandlerIntentPickerView(
      const WebAppProtocolHandlerIntentPickerView&) = delete;
  WebAppProtocolHandlerIntentPickerView& operator=(
      const WebAppProtocolHandlerIntentPickerView&) = delete;
  ~WebAppProtocolHandlerIntentPickerView() override;

  static void Show(const GURL& url,
                   Profile* profile,
                   const base::CommandLine& command_line,
                   std::vector<std::string> app_ids,
                   base::OnceCallback<void(bool accepted)> close_callback);

 private:
  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  const std::string& GetSelectedAppId() const;
  void OnAccepted();
  void OnCanceled();
  void OnClosed();
  void Initialize();

  // Unselects the current focused app item on the list and
  // refocus on the selected app item based on the index provided.
  void SetSelectedAppIndex(size_t index, const ui::Event& event);

  // Runs the close_callback_ provided during Show() if it exists.
  void RunCloseCallback(bool accepted);

  const GURL url_;
  Profile* const profile_;
  const base::CommandLine command_line_;
  const std::vector<std::string> app_ids_;
  base::OnceCallback<void(bool)> close_callback_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  std::vector<WebAppHoverButton*> hover_buttons_;
  views::Checkbox* remember_selection_checkbox_ = nullptr;
  views::ScrollView* scroll_view_ = nullptr;

  // Pre-select the first app on the list.
  size_t selected_app_tag_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_
