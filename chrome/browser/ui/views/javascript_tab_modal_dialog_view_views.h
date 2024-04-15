// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_TAB_MODAL_DIALOG_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_TAB_MODAL_DIALOG_VIEW_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "components/javascript_dialogs/tab_modal_dialog_view.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

// A Views version of a JavaScript dialog that automatically dismisses itself
// when the user switches away to a different tab, used for WebContentses that
// are browser tabs.
class JavaScriptTabModalDialogViewViews
    : public javascript_dialogs::TabModalDialogView,
      public views::DialogDelegateView {
  METADATA_HEADER(JavaScriptTabModalDialogViewViews, views::DialogDelegateView)

 public:
  JavaScriptTabModalDialogViewViews(const JavaScriptTabModalDialogViewViews&) =
      delete;
  JavaScriptTabModalDialogViewViews& operator=(
      const JavaScriptTabModalDialogViewViews&) = delete;
  ~JavaScriptTabModalDialogViewViews() override;

  // JavaScriptDialog:
  void CloseDialogWithoutCallback() override;
  std::u16string GetUserInput() override;

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  views::View* GetInitiallyFocusedView() override;

  // views::View:
  void AddedToWidget() override;

  // TODO(crbug.com/40843165): We cannot use unique_ptr because ownership of
  // this object gets passed to Views.
  static JavaScriptTabModalDialogViewViews* CreateAlertDialogForTesting(
      Browser* browser,
      std::u16string title,
      std::u16string message);

 private:
  friend class JavaScriptDialog;
  friend class JavaScriptTabModalDialogManagerDelegateDesktop;

  JavaScriptTabModalDialogViewViews(
      content::WebContents* parent_web_contents,
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_force_closed_callback);

  std::u16string title_;
  std::u16string message_text_;
  std::u16string default_prompt_text_;
  content::JavaScriptDialogManager::DialogClosedCallback dialog_callback_;
  base::OnceClosure dialog_force_closed_callback_;

  // The message box view whose commands we handle.
  raw_ptr<views::MessageBoxView> message_box_view_;

  base::WeakPtrFactory<JavaScriptTabModalDialogViewViews> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_TAB_MODAL_DIALOG_VIEW_VIEWS_H_
