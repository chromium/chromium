// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_REQUEST_PIN_VIEW_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_REQUEST_PIN_VIEW_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/certificate_provider/security_token_pin_dialog_host.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}  // namespace views

// A dialog box for requesting PIN code. Instances of this class are managed by
// SecurityTokenPinDialogHostPopupImpl.
class RequestPinView final : public views::DialogDelegateView,
                             public views::TextfieldController {
  METADATA_HEADER(RequestPinView, views::DialogDelegateView)

 public:
  using PinEnteredCallback =
      base::RepeatingCallback<void(const std::string& user_input)>;
  using ViewDestructionCallback = base::OnceClosure;

  // Creates the view to be embedded in the dialog that requests the PIN/PUK.
  // |extension_name| - the name of the extension making the request. Displayed
  //     in the title and in the header of the view.
  // |code_type| - the type of code requested, PIN or PUK.
  // |attempts_left| - the number of attempts user has to try the code. When
  //     zero the textfield is disabled and user cannot provide any input. When
  //     -1 the user is allowed to provide the input and no information about
  //     the attepts left is displayed in the view.
  // |pin_entered_callback| - called every time the user submits the input.
  // |view_destruction_callback| - called by the destructor.
  RequestPinView(const std::string& extension_name,
                 chromeos::security_token_pin::CodeType code_type,
                 int attempts_left,
                 const PinEnteredCallback& pin_entered_callback,
                 ViewDestructionCallback view_destruction_callback);
  RequestPinView(const RequestPinView&) = delete;
  RequestPinView& operator=(const RequestPinView&) = delete;
  ~RequestPinView() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // views::DialogDelegateView:
  bool Accept() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  std::u16string GetWindowTitle() const override;

  // |code_type| - specifies whether the user is asked to enter PIN or PUK.
  // |error_label| - the error template to be displayed in red in the dialog. If
  //     |kNone|, no error is displayed.
  // |attempts_left| - included in the view as the number of attepts user can
  //     have to enter correct code.
  // |accept_input| - specifies whether the textfield is enabled. If disabled
  //     the user is unable to provide input.
  void SetDialogParameters(chromeos::security_token_pin::CodeType code_type,
                           chromeos::security_token_pin::ErrorLabel error_label,
                           int attempts_left,
                           bool accept_input);

  // Set the name of extension that is using this view. The name is included in
  // the header text displayed by the view.
  void SetExtensionName(const std::string& extension_name);

  // Checking that the text style of `error_label_` is correct.
  bool IsTextStyleOfErrorLabelCorrectForTesting() const;

  views::Textfield* textfield_for_testing() { return textfield_; }

 private:
  // This initializes the view, with all the UI components.
  void Init();
  void SetAcceptInput(bool accept_input);
  void SetErrorMessage(chromeos::security_token_pin::ErrorLabel error_label,
                       int attempts_left,
                       bool accept_input);
  // Updates the header text |header_label_| based on values from
  // |window_title_| and |code_type_|.
  void UpdateHeaderText();

  const PinEnteredCallback pin_entered_callback_;
  ViewDestructionCallback view_destruction_callback_;

  // Whether the UI is locked, disallowing the user to input any data, while the
  // caller processes the previously entered PIN/PUK.
  bool locked_ = false;

  std::u16string window_title_;
  raw_ptr<views::Label> header_label_ = nullptr;
  std::u16string code_type_;
  raw_ptr<views::Textfield> textfield_ = nullptr;
  raw_ptr<views::Label> error_label_ = nullptr;

  base::WeakPtrFactory<RequestPinView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_REQUEST_PIN_VIEW_CHROMEOS_H_
