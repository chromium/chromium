// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_DELEGATE_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

class TabModalConfirmDialogCloseDelegate {
 public:
  TabModalConfirmDialogCloseDelegate() {}

  TabModalConfirmDialogCloseDelegate(
      const TabModalConfirmDialogCloseDelegate&) = delete;
  TabModalConfirmDialogCloseDelegate& operator=(
      const TabModalConfirmDialogCloseDelegate&) = delete;

  virtual ~TabModalConfirmDialogCloseDelegate() {}

  virtual void CloseDialog() = 0;
};

// This class acts as the delegate for a simple tab-modal dialog confirming
// whether the user wants to execute a certain action.
class TabModalConfirmDialogDelegate : public content::WebContentsObserver {
 public:
  explicit TabModalConfirmDialogDelegate(content::WebContents* web_contents);

  TabModalConfirmDialogDelegate(const TabModalConfirmDialogDelegate&) = delete;
  TabModalConfirmDialogDelegate& operator=(
      const TabModalConfirmDialogDelegate&) = delete;

  ~TabModalConfirmDialogDelegate() override;

  void set_close_delegate(TabModalConfirmDialogCloseDelegate* close_delegate) {
    close_delegate_ = close_delegate;
  }

  // Accepts the confirmation prompt and calls OnAccepted() if no other call
  // to Accept(), Cancel() or Close() has been made before.
  // This method is safe to call even from an OnAccepted(), OnCanceled(),
  // OnClosed() or OnLinkClicked() callback.
  void Accept();

  // Cancels the confirmation prompt and calls OnCanceled() if no other call
  // to Accept(), Cancel() or Close() has been made before.
  // This method is safe to call even from an OnAccepted(), OnCanceled(),
  // OnClosed() or OnLinkClicked() callback.
  void Cancel();

  // Called when the dialog is closed without selecting an option, e.g. by
  // pressing the close button on the dialog, using a window manager gesture,
  // closing the parent tab or navigating in the parent tab.
  // Calls OnClosed() and closes the dialog if no other call to Accept(),
  // Cancel() or Close() has been made before.
  // This method is safe to call even from an OnAccepted(), OnCanceled(),
  // OnClosed() or OnLinkClicked() callback.
  void Close();

  // Called when the link is clicked. Calls OnLinkClicked() if the dialog is
  // not in the process of closing. The |disposition| specifies how the
  // resulting document should be loaded (based on the event flags present when
  // the link was clicked).
  void LinkClicked(WindowOpenDisposition disposition);

  // The title of the dialog. Note that the title is not shown on all platforms.
  virtual std::u16string GetTitle() = 0;
  virtual std::u16string GetDialogMessage() = 0;

  // Icon to show for the dialog. If this method is not overridden, a default
  // icon (like the application icon) is shown.
  virtual gfx::Image* GetIcon();

  // Title for the accept and the cancel buttons.
  // The default implementation uses IDS_OK and IDS_CANCEL.
  virtual int GetDialogButtons() const;
  virtual std::u16string GetAcceptButtonTitle();
  virtual std::u16string GetCancelButtonTitle();

  // Returns the text of the link to be displayed, if any. Otherwise returns
  // an empty string.
  virtual std::u16string GetLinkText() const;

  // GTK stock icon names for the accept and cancel buttons, respectively.
  // The icons are only used on GTK. If these methods are not overriden,
  // the buttons have no stock icons.
  virtual const char* GetAcceptButtonIcon();
  virtual const char* GetCancelButtonIcon();

  // Allow the delegate to customize which button is default, and which is
  // initially focused. If returning std::nullopt, the dialog uses default
  // behavior.
  virtual std::optional<int> GetDefaultDialogButton();
  virtual std::optional<int> GetInitiallyFocusedButton();

  // content::WebContentObserver:
  void DidStartLoading() override;

 protected:
  TabModalConfirmDialogCloseDelegate* close_delegate() {
    return close_delegate_;
  }

 private:
  // It is guaranteed that exactly one of OnAccepted(), OnCanceled() or
  // OnClosed() is eventually called. These method are private to enforce this
  // guarantee. Access to them is controlled by Accept(), Cancel() and Close().

  // Called when the user accepts or cancels the dialog, respectively.
  virtual void OnAccepted();
  virtual void OnCanceled();

  // Called when the dialog is closed.
  virtual void OnClosed();

  // Called when the link is clicked. Acces to the method is controlled by
  // LinkClicked(), which checks that the dialog is not in the process of
  // closing. It's correct to close the dialog by calling Accept(), Cancel()
  // or Close() from this callback.
  virtual void OnLinkClicked(WindowOpenDisposition disposition);

  // Close the dialog.
  void CloseDialog();

  raw_ptr<TabModalConfirmDialogCloseDelegate> close_delegate_;

  // True iff we are in the process of closing, to avoid running callbacks
  // multiple times.
  bool closing_;
};

#endif  // CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_DELEGATE_H_
