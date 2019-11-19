// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_WIN_TRY_CHROME_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_WIN_TRY_CHROME_DIALOG_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/installer/util/experiment_metrics.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class SingletonHwndObserver;
}

namespace views {
class View;
class Widget;
}  // namespace views

// This class displays a modal dialog using the views system. The dialog asks
// the user to give Chrome another try. This class only handles the UI so the
// resulting actions are up to the caller.
//
// The layout is as follows:
//
//   +-----------------------------------------------+
//   | |icon| Header text.                       [x] |
//   |                                               |
//   |        Body text.                             |
//   |        [ Open Chrome ] [No Thanks]            |
//   +-----------------------------------------------+
//
// Some variants do not have body text, or only have one button.
class TryChromeDialog : public views::ButtonListener,
                        public views::WidgetObserver,
                        public ui::EventHandler {
 public:
  // Receives a closure to run upon process singleton notification when the
  // modal dialog is open, or a null closure when the active dialog is
  // dismissed.
  using ActiveModalDialogListener = base::Callback<void(base::Closure)>;

  enum Result {
    NOT_NOW,                    // Don't launch chrome. Exit now.
    OPEN_CHROME_WELCOME,        // Launch Chrome to the standard Welcome page.
    OPEN_CHROME_DEFAULT,        // Launch Chrome to the default page.
    OPEN_CHROME_DEFER,          // Launch Chrome on account of a rendezvous,
                                // deferring to the caller's command line.
  };

  // Shows a modal dialog asking the user to give Chrome another try. See
  // above for the possible outcomes of the function.
  // |group| selects what strings to present and what controls are shown.
  // |listener| will be provided with a closure when the modal event loop is
  // started and when it completes.
  // Note that the dialog has no parent and it will position itself in a lower
  // corner of the screen or near the Chrome taskbar button.
  // The dialog does not steal focus and does not have an entry in the taskbar.
  static Result Show(size_t group, ActiveModalDialogListener listener);

  ~TryChromeDialog() override;

 private:
  class Delegate {
   public:
    // Called to tell the delegate that the dialog was shown at
    // |toast_location|.
    virtual void SetToastLocation(
        installer::ExperimentMetrics::ToastLocation toast_location) {}

    // Called to tell the delegate that the experiment has entered |state|.
    virtual void SetExperimentState(installer::ExperimentMetrics::State state) {
    }

    // Called to tell the delegate that the interaction with the toast has
    // completed.
    virtual void InteractionComplete() {}

   protected:
    virtual ~Delegate() {}
  };

  class Context;
  class ModalShowDelegate;

  friend class TryChromeDialogBrowserTestBase;

  // Creates a Try Chrome toast dialog. |group| signifies an experiment group
  // which dictactes messaging text and presence of ui elements. |delegate|,
  // which must outlive the instance, is notified of relevant details throughout
  // the interaction.
  TryChromeDialog(size_t group, Delegate* delegate);

  // Starts the process of presenting the dialog by initiating an asychronous
  // search for Chrome's taskbar icon via the encapsulated context object.
  void ShowDialogAsync();

  // Continues work to show the toast following asynchronous context
  // initialization. The interaction is completed immediately in case of
  // rendezvous to allow browser startup to continue. Otherwise, the dialog is
  // shown to the user. The delegate's SetToastLocation method is called with
  // the location.
  void OnContextInitialized();

  // Notifies the delegate of the final experiment state and that the
  // interaction has completed.
  void CompleteInteraction();

  // Invoked upon notification from another process by way of the process
  // singleton. Triggers completion of the interaction by closing the dialog.
  void OnProcessNotification();

  // Handles for events sent by the dialog's Widget.
  void GainedMouseHover();
  void LostMouseHover();

  // views::ButtonListener:
  // Updates the result_ and state_ based on which button was pressed and
  // closes the dialog.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetCreated(views::Widget* widget) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  Result result() const { return result_; }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

  // A gfx::SingletonHwndObserver::WndProc for handling WM_ENDSESSION messages.
  void OnWindowMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

  views::Widget* widget() { return popup_; }

  // Controls which experiment group to use for varying the layout and controls.
  const size_t group_;
  Delegate* const delegate_;

  std::unique_ptr<Context> context_;

  // A closure to run when the interaction has completed.
  base::Closure on_complete_;

  // The pessimistic result that will prevent launching Chrome.
  Result result_ = NOT_NOW;

  // An observer to handle WM_ENDSESSION messages by updating the experiment
  // state accordingly.
  std::unique_ptr<gfx::SingletonHwndObserver> endsession_observer_;

  // The pessimistic state indicating that the dialog was closed via some means
  // other than its intended UX.
  installer::ExperimentMetrics::State state_ =
      installer::ExperimentMetrics::kOtherClose;

  // Unowned; |popup_| owns itself.
  views::Widget* popup_ = nullptr;

  // The close button; owned by |popup_|.
  views::View* close_button_ = nullptr;

  // True when the mouse is considered to be hovering over the dialog.
  bool has_hover_ = false;

  SEQUENCE_CHECKER(my_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(TryChromeDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_WIN_TRY_CHROME_DIALOG_H_
