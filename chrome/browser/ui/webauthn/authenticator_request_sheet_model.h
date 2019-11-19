// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_

#include <memory>

#include "base/optional.h"
#include "base/strings/string16.h"

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

  virtual bool IsCancelButtonVisible() const = 0;
  virtual base::string16 GetCancelButtonLabel() const = 0;

  virtual bool IsAcceptButtonVisible() const = 0;
  virtual bool IsAcceptButtonEnabled() const = 0;
  virtual base::string16 GetAcceptButtonLabel() const = 0;

  virtual const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const = 0;
  virtual base::string16 GetStepTitle() const = 0;
  virtual base::string16 GetStepDescription() const = 0;
  virtual base::Optional<base::string16> GetAdditionalDescription() const = 0;

  virtual ui::MenuModel* GetOtherTransportsMenuModel() = 0;

  virtual void OnBack() = 0;
  virtual void OnAccept() = 0;
  virtual void OnCancel() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_
