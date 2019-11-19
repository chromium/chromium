// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "ui/views/controls/button/button.h"

namespace views {
class View;
}

namespace payments {

class PaymentRequestDialogView;
class PaymentRequestSpec;
class PaymentRequestState;

// The base class for objects responsible for the creation and event handling in
// views shown in the PaymentRequestDialog.
class PaymentRequestSheetController : public views::ButtonListener {
 public:
  // Objects of this class are owned by |dialog|, so it's a non-owned pointer
  // that should be valid throughout this object's lifetime.
  // |state| and |spec| are also not owned by this and are guaranteed to outlive
  // dialog. Neither |state|, |spec| or |dialog| should be null.
  PaymentRequestSheetController(PaymentRequestSpec* spec,
                                PaymentRequestState* state,
                                PaymentRequestDialogView* dialog);
  ~PaymentRequestSheetController() override;

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

  PaymentRequestSpec* spec() { return spec_; }
  PaymentRequestState* state() { return state_; }

  // The dialog that contains and owns this object.
  // Caller should not take ownership of the result.
  PaymentRequestDialogView* dialog() { return dialog_; }

  // Returns the title to be displayed in this sheet's header.
  virtual base::string16 GetSheetTitle() = 0;

  // Stops the controller from controlling the UI. Used when the UI is being
  // destroyed.
  void Stop() { is_active_ = false; }

 protected:
  // Clears the content part of the view represented by this view controller and
  // calls FillContentView again to re-populate it with updated views.
  void UpdateContentView();

  // Clears and recreates the header view for this sheet.
  void UpdateHeaderView();

  // Clears and recreates the header content separator view for this sheet.
  void UpdateHeaderContentSeparatorView();

  // Update the focus to |focused_view|.
  void UpdateFocus(views::View* focused_view);

  // View controllers should call this if they have modified some layout aspect
  // (e.g., made it taller or shorter), and want to relayout the whole pane.
  void RelayoutPane();

  // Creates and returns the primary action button for this sheet. It's
  // typically a blue button with the "Pay" or "Done" labels. Subclasses may
  // return an empty std::unique_ptr (nullptr) to indicate that no primary
  // button should be displayed. The caller takes ownership of the button but
  // the view is guaranteed to be outlived by the controller so subclasses may
  // retain a raw pointer to the returned button (for example to control its
  // enabled state).
  virtual std::unique_ptr<views::Button> CreatePrimaryButton();

  // Returns the text that should be on the secondary button, by default
  // "Cancel".
  virtual base::string16 GetSecondaryButtonLabel();

  // Returns the secondary button tag, by default
  // static_cast<int>(PaymentRequestCommonTags::CLOSE_BUTTON_TAG).
  virtual int GetSecondaryButtonTag();

  // Returns the secondary button id, by default
  // static_cast<int>(DialogViewID::CANCEL_BUTTON).
  virtual int GetSecondaryButtonId();

  // Returns true if the secondary button should be shown, false otherwise.
  virtual bool ShouldShowSecondaryButton();

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

  // Creates and returns the view to be inserted in the header, next to the
  // close/back button. This is typically the sheet's title but it can be
  // overriden to return a different kind of view as long as it fits inside the
  // header.
  virtual std::unique_ptr<views::View> CreateHeaderContentView(
      views::View* header_view);

  // Creates and returns the view to be inserted in the header content separator
  // container betweem header and content.
  virtual views::View* CreateHeaderContentSeparatorView();

  // Returns the background to use for the header section of the sheet.
  virtual std::unique_ptr<views::Background> GetHeaderBackground(
      views::View* header_view);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

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

  views::Button* primary_button() { return primary_button_; }

  // Returns whether the controller should be controlling the UI.
  bool is_active() const { return is_active_; }

 private:
  // Called when the Enter accelerator is pressed. Perform the action associated
  // with the primary button and sets |is_enabled| to true if it's enabled,
  // otherwise sets it to false. The |is_enabled| is an out-param to enable
  // binding the method with a base::WeakPtr, which prohibits non-void return
  // values.
  void PerformPrimaryButtonAction(bool* is_enabled);

  // Add the primary/secondary buttons to |container|.
  void AddPrimaryButton(views::View* container);
  void AddSecondaryButton(views::View* container);

  // All these are not owned. Will outlive this.
  PaymentRequestSpec* spec_ = nullptr;
  PaymentRequestState* state_ = nullptr;
  PaymentRequestDialogView* dialog_ = nullptr;

  // This view is owned by its encompassing ScrollView.
  views::View* pane_ = nullptr;
  views::View* content_view_ = nullptr;

  // Hold on to the ScrollView because it must be explicitly laid out in some
  // cases.
  views::ScrollView* scroll_ = nullptr;

  // Hold on to the primary and secondary buttons to use them as initial focus
  // targets when subclasses don't want to focus anything else.
  views::Button* primary_button_ = nullptr;
  views::Button* secondary_button_ = nullptr;
  views::View* header_view_ = nullptr;
  views::View* header_content_separator_container_ = nullptr;

  // Whether the controller should be controlling the UI.
  bool is_active_ = true;

  base::WeakPtrFactory<PaymentRequestSheetController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestSheetController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
