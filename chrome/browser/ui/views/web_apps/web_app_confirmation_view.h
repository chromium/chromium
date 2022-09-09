// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_CONFIRMATION_VIEW_H_

#include <string>

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Textfield;
class RadioButton;
}  // namespace views

// WebAppConfirmationView provides views for editing the details to
// create a web app with. (More tools > Add to desktop)
class WebAppConfirmationView : public views::DialogDelegateView,
                               public views::TextfieldController {
 public:
  METADATA_HEADER(WebAppConfirmationView);
  WebAppConfirmationView(std::unique_ptr<WebAppInstallInfo> web_app_info,
                         chrome::AppInstallationAcceptanceCallback callback);
  WebAppConfirmationView(const WebAppConfirmationView&) = delete;
  WebAppConfirmationView& operator=(const WebAppConfirmationView&) = delete;
  ~WebAppConfirmationView() override;

 private:
  // Overridden from views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // Overriden from views::DialogDelegateView:
  bool Accept() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;

  // Overridden from views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // Get the trimmed contents of the title text field.
  std::u16string GetTrimmedTitle() const;

  // The WebAppInstallInfo that the user is editing.
  // Cleared when the dialog completes (Accept/WindowClosing).
  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  // The callback to be invoked when the dialog is completed.
  chrome::AppInstallationAcceptanceCallback callback_;

  // Checkbox to launch as a window.
  views::Checkbox* open_as_window_checkbox_ = nullptr;

  // Radio buttons to launch as a tab, window or tabbed window.
  views::RadioButton* open_as_tab_radio_ = nullptr;
  views::RadioButton* open_as_window_radio_ = nullptr;
  views::RadioButton* open_as_tabbed_window_radio_ = nullptr;

  // Textfield showing the title of the app.
  views::Textfield* title_tf_ = nullptr;
};

BEGIN_VIEW_BUILDER(, WebAppConfirmationView, views::DialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, WebAppConfirmationView)

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_CONFIRMATION_VIEW_H_
