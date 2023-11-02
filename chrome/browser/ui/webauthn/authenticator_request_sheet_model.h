// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_

#include <memory>
#include <string>


namespace gfx {
struct VectorIcon;
}

namespace ui {
class MenuModel;
}

// The basic interface of models backing a given UI sheet shown in the WebAuthn
// request dialog; where each sheet, in turn, corresponds to one of `steps`
// defined by AuthenticatorRequestDialogModel.
//
// For each step, the model implementation encapsulates:
//
//  (1) knowledge of the set of actions possible to the user at that step,
//
//  (2) pieces of data required by views to visualise the sheet:
//    (a) strings to use on labels/buttons rendered by the SheetView and the
//        AuthenticatorRequestDialogView, and the state of the buttons,
//    (b) data for additional step-specific contents rendered by SheetView
//        subclasses, if any,
//
//  (3) logic to handle user interactions with:
//    (a) the `Back`, `Accept`, `Cancel`, buttons, even though the latter two
//    are actually rendered by the AuthenticatorRequestDialogView,
//    (b) the step-specific contents, if any.
//
class AuthenticatorRequestSheetModel {
 public:
  // Indicates what style to pick for the step illustration.
  enum class ImageColorScheme { kDark, kLight };

  virtual ~AuthenticatorRequestSheetModel() {}

  virtual bool IsActivityIndicatorVisible() const = 0;
  virtual bool IsBackButtonVisible() const = 0;
  virtual bool ShouldFocusBackArrow() const;
  virtual bool IsCloseButtonVisible() const;

  virtual bool IsCancelButtonVisible() const = 0;
  virtual std::u16string GetCancelButtonLabel() const = 0;

  virtual bool IsAcceptButtonVisible() const = 0;
  virtual bool IsAcceptButtonEnabled() const = 0;
  virtual std::u16string GetAcceptButtonLabel() const = 0;

  virtual bool IsManageDevicesButtonVisible() const;
  virtual bool IsOtherMechanismButtonVisible() const;

  virtual const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const = 0;
  virtual std::u16string GetStepTitle() const = 0;
  virtual std::u16string GetStepDescription() const = 0;
  virtual std::u16string GetAdditionalDescription() const;
  virtual std::u16string GetError() const;

  virtual ui::MenuModel* GetOtherMechanismsMenuModel();

  virtual void OnBack() = 0;
  virtual void OnAccept() = 0;
  virtual void OnCancel() = 0;
  virtual void OnManageDevices();
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_
