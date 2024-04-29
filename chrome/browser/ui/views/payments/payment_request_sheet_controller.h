// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "ui/views/controls/button/button.h"

namespace ui {
class Event;
}

namespace views {
class MdTextButton;
class View;
}

namespace payments {

class PaymentRequestDialogView;
class PaymentRequestSpec;
class PaymentRequestState;

// The base class for objects responsible for the creation and event handling in
// views shown in the PaymentRequestDialog.
class PaymentRequestSheetController {
 public:
  using ButtonCallback = views::Button::PressedCallback::Callback;

  // Objects of this class are owned by |dialog|, so it's a non-owned pointer
  // that should be valid throughout this object's lifetime.
  // |state| and |spec| are also not owned by this and are guaranteed to outlive
  // dialog. Neither |state|, |spec| or |dialog| should be null.
  PaymentRequestSheetController(base::WeakPtr<PaymentRequestSpec> spec,
                                base::WeakPtr<PaymentRequestState> state,
                                base::WeakPtr<PaymentRequestDialogView> dialog);

  PaymentRequestSheetController(const PaymentRequestSheetController&) = delete;
  PaymentRequestSheetController& operator=(
      const PaymentRequestSheetController&) = delete;

  virtual ~PaymentRequestSheetController();

  // Creates a view to be displayed in the PaymentRequestDialog. The header view
  // is the view displayed on top of the dialog, containing title, (optional)
  // back button, and close buttons.
  // The content view is displayed between the header view and the pay/cancel
  // buttons. Also adds the footer, returned by CreateFooterView(), which is
  // clamped to the bottom of the containing view.  The returned view takes
  // ownership of the header, the content, and the footer.
  // +---------------------------+
  // |        HEADER VIEW        |
  // +---------------------------+
  // |          CONTENT          |
  // |           VIEW            |
  // +---------------------------+
  // | EXTRA VIEW | PAY | CANCEL | <-- footer
  // +---------------------------+
  std::unique_ptr<views::View> CreateView();

  base::WeakPtr<PaymentRequestSpec> spec() { return spec_; }
  base::WeakPtr<PaymentRequestState> state() { return state_; }

  // The dialog that contains and owns this object.
  // Caller should not take ownership of the result.
  base::WeakPtr<PaymentRequestDialogView> dialog() { return dialog_; }

  // Returns the title to be displayed in this sheet's header.
  virtual std::u16string GetSheetTitle() = 0;

  // Stops the controller from controlling the UI. Used when the UI is being
  // destroyed.
  virtual void Stop();

  // Called when the back button is pressed on the dialog.
  void BackButtonPressed();

  // Called when the close button is pressed on the dialog.
  void CloseButtonPressed(const ui::Event& event);

 protected:
  // Clears the content part of the view represented by this view controller and
  // calls FillContentView again to re-populate it with updated views.
  void UpdateContentView();

  // Clears and recreates the header view for this sheet.
  void UpdateHeaderView();

  // Update the focus to |focused_view|.
  void UpdateFocus(views::View* focused_view);

  // View controllers should call this if they have modified some layout aspect
  // (e.g., made it taller or shorter), and want to relayout the whole pane.
  void RelayoutPane();

  // Methods that control the appearance and behavior of the primary dialog
  // button.  By default the dialog shows a "pay" button.
  virtual bool ShouldShowPrimaryButton();
  virtual std::u16string GetPrimaryButtonLabel();
  virtual ButtonCallback GetPrimaryButtonCallback();
  virtual int GetPrimaryButtonId();
  virtual bool GetPrimaryButtonEnabled();

  // Methods that control the appearance and behavior of the secondary dialog
  // button.  By default the dialog shows a "cancel payment" button.
  virtual bool ShouldShowSecondaryButton();
  virtual std::u16string GetSecondaryButtonLabel();
  virtual ButtonCallback GetSecondaryButtonCallback();
  virtual int GetSecondaryButtonId();

  // Returns whether this sheet should display a back arrow in the header next
  // to the title.
  virtual bool ShouldShowHeaderBackArrow();

  // Implemented by subclasses to populate |content_view| with the views that
  // should be displayed in their content area (between the header and the
  // footer). This may be called at view creation time as well as anytime
  // UpdateContentView is called.
  virtual void FillContentView(views::View* content_view) = 0;

  // Creates and returns the view to be displayed next to the "Pay" and "Cancel"
  // buttons. May return an empty std::unique_ptr (nullptr) to indicate that no
  // extra view is to be displayed.The caller takes ownership of the view but
  // the view is guaranteed to be outlived by the controller so subclasses may
  // retain a raw pointer to the returned view (for example to control its
  // enabled state). The horizontal and vertical insets (to the left and bottom
  // borders) is taken care of by the caller, so can be set to 0.
  // +---------------------------+
  // | EXTRA VIEW | PAY | CANCEL |
  // +---------------------------+
  virtual std::unique_ptr<views::View> CreateExtraFooterView();

  // Creates and returns a header for all the sheets in the PaymentRequest
  // dialog. The header contains an optional back arrow button (if
  // |ShouldShowHeaderBackArrow| returns true) and a content view created by
  // |CreateHeaderContentView|. The background is set based on
  // |GetHeaderBackground|, and its color is used to decide which color to use
  // to paint the arrow.
  //
  // The passed-in `view` must be the `header_view_` - it is only passed as an
  // argument because this is required by ViewFactory.
  //
  // +---------------------------+
  // | <- | header_content_view  |
  // +---------------------------+
  virtual void PopulateSheetHeaderView(views::View* view);

  // Creates the row of button containing the Pay, cancel, and extra buttons.
  // |controller| is installed as the listener for button events.
  std::unique_ptr<views::View> CreateFooterView();

  // Returns the view that should be initially focused on this sheet. Typically,
  // this returns the primary button if it's enabled or the secondary button
  // otherwise. Subclasses may return a different view if they need focus to
  // start off on a different view (a textfield for example). This will only be
  // called after the view has been completely created through calls to
  // CreatePaymentView and related functions.
  virtual views::View* GetFirstFocusedView();

  // Returns true if the subclass wants the content sheet to have an id, and
  // sets |sheet_id| to the desired value.
  virtual bool GetSheetId(DialogViewID* sheet_id);

  // Returns true to display dynamic top and bottom border for hidden contents.
  virtual bool DisplayDynamicBorderForHiddenContents();

  // Returns true if the subclass wants the 'Enter' key to be accelerated to
  // always map to performing the primary button action (irregardless of the
  // currently focused element). If a subclass returns true for this, it must
  // also return true for ShouldShowPrimaryButton.
  virtual bool ShouldAccelerateEnterKey();

  // Returns the height of the active header view.
  int GetHeaderHeight();

  // Returns true if the content view should be placed within a scrollable view
  // that will show a vertical scrollbar if the content is taller than the
  // payment sheet.
  virtual bool CanContentViewBeScrollable();

  views::MdTextButton* primary_button() { return primary_button_; }

  views::View* header_content_separator_container() {
    return header_content_separator_container_;
  }

  views::View* content_view() { return content_view_; }

  // Returns whether the controller should be controlling the UI.
  bool is_active() const { return is_active_; }

  // Provide a base::WeakPtr to the subclass instance. Subclasses must implement
  // this method as a base::WeakPtrFactory must be the last member in the
  // concrete (aka leaf) class in order to avoid subtle use-after-destroy
  // issues.
  virtual base::WeakPtr<PaymentRequestSheetController> GetWeakPtr() = 0;

 private:
  // Add the primary/secondary buttons to |container|.
  void AddPrimaryButton(views::View* container);
  void AddSecondaryButton(views::View* container);

  // Called when the Enter accelerator is pressed. Perform the action associated
  // with the primary button and sets |is_enabled| to true if it's enabled,
  // otherwise sets it to false. The |is_enabled| is an out-param to enable
  // binding the method with a base::WeakPtr, which prohibits non-void return
  // values.
  void PerformPrimaryButtonAction(bool* is_enabled, const ui::Event& event);

  base::WeakPtr<PaymentRequestSpec> const spec_;
  base::WeakPtr<PaymentRequestState> const state_;
  base::WeakPtr<PaymentRequestDialogView> const dialog_;

  // This view is owned by its encompassing ScrollView.
  raw_ptr<views::View, DanglingUntriaged> pane_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> content_view_ = nullptr;

  // Hold on to the ScrollView because it must be explicitly laid out in some
  // cases.
  raw_ptr<views::ScrollView, DanglingUntriaged> scroll_ = nullptr;

  // Hold on to the primary and secondary buttons to use them as initial focus
  // targets when subclasses don't want to focus anything else.
  raw_ptr<views::MdTextButton, DanglingUntriaged> primary_button_ = nullptr;
  raw_ptr<views::Button, DanglingUntriaged> secondary_button_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> header_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> header_content_separator_container_ =
      nullptr;

  // Whether the controller should be controlling the UI.
  bool is_active_ = true;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
