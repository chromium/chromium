// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_DIALOG_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_DIALOG_MANAGER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager_delegate.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

class DocumentPipJavaScriptDialogView;

// TabModalDialogManagerDelegate for the standalone Document Picture-in-Picture
// window. JavaScript dialogs (alert/confirm/prompt) raised by the PiP document
// are routed here.
//
// Unlike JavaScriptTabModalDialogManagerDelegateDesktop, this delegate does not
// depend on Browser / TabStripModel / TabInterface: the PiP child WebContents
// is not a tab in any browser's tab strip. Dialogs are parented directly to the
// PiP WebContents as child-modal dialogs through its Browser-free modal host.
class DocumentPipDialogManagerDelegate
    : public javascript_dialogs::TabModalDialogManagerDelegate {
 public:
  // `pip_widget` is the floating PiP widget used to check window state.
  // Dialogs are parented through the PiP WebContents modal host. The widget's
  // storage outlives this delegate (the delegate is owned by the
  // TabModalDialogManager attached to the PiP child WebContents, and is
  // destroyed during the widget's teardown when that child WebContents is
  // destroyed), but the widget may already be closing by then. Callers must
  // guard widget access with IsClosed() rather than assume it is still
  // functional.
  explicit DocumentPipDialogManagerDelegate(views::Widget* pip_widget);

  DocumentPipDialogManagerDelegate(const DocumentPipDialogManagerDelegate&) =
      delete;
  DocumentPipDialogManagerDelegate& operator=(
      const DocumentPipDialogManagerDelegate&) = delete;

  ~DocumentPipDialogManagerDelegate() override;

  // javascript_dialogs::TabModalDialogManagerDelegate:
  base::WeakPtr<javascript_dialogs::TabModalDialogView> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) override;
  void WillRunDialog() override;
  void DidCloseDialog() override;
  void SetTabNeedsAttention(bool attention) override;
  bool IsWebContentsForemost() override;
  bool IsApp() override;
  bool CanShowModalUI() override;

  // Returns the Widget backing the currently-active dialog, or null if no
  // dialog is showing. Tests use this to drive the dialog cross-platform
  // instead of walking the native owned-widget hierarchy, which is unreliable
  // on platforms where modal parenting is not fully wired up (e.g. desktop
  // Linux in unit tests).
  views::Widget* GetActiveDialogWidgetForTesting();

 private:
  // The floating PiP widget used to check window state.
  const raw_ref<views::Widget> pip_widget_;
  std::unique_ptr<DocumentPipJavaScriptDialogView> active_dialog_;

  base::WeakPtrFactory<DocumentPipDialogManagerDelegate> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_DIALOG_MANAGER_DELEGATE_H_
