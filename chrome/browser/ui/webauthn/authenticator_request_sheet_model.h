// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_

#include <optional>
#include <string>

namespace gfx {
struct VectorIcon;
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
//    (a) the `Accept` and `Cancel` buttons, even though thouse are actually
//        rendered by the AuthenticatorRequestDialogView,
//    (b) the step-specific contents, if any.
//
class AuthenticatorRequestSheetModel {
 public:
  // IllustrationPair contains a pair of illustrations: one for light mode and
  // one for dark mode.
  template <typename T>
  struct IllustrationPair {
    IllustrationPair(T in_light, T in_dark) : light(in_light), dark(in_dark) {}
    T get(bool is_dark) const { return is_dark ? dark : light; }
    const T light, dark;
  };

  virtual ~AuthenticatorRequestSheetModel() = default;

  virtual bool IsActivityIndicatorVisible() const = 0;

  virtual bool IsCancelButtonVisible() const = 0;
  virtual std::u16string GetCancelButtonLabel() const = 0;

  virtual bool IsAcceptButtonVisible() const = 0;
  virtual bool IsAcceptButtonEnabled() const = 0;
  virtual std::u16string GetAcceptButtonLabel() const = 0;

  virtual bool IsManageDevicesButtonVisible() const;
  virtual bool IsOtherMechanismButtonVisible() const;
  virtual bool IsForgotGPMPinButtonVisible() const;
  virtual bool IsGPMPinOptionsButtonVisible() const;
  virtual std::u16string GetOtherMechanismButtonLabel() const;

  virtual std::u16string GetStepTitle() const = 0;
  virtual std::u16string GetStepDescription() const = 0;
  virtual std::u16string GetAdditionalDescription() const;
  virtual std::u16string GetError() const;

  virtual void OnBack() = 0;
  virtual void OnAccept() = 0;
  virtual void OnCancel() = 0;
  virtual void OnManageDevices();
  virtual void OnForgotGPMPin() const;
  virtual void OnGPMPinOptionChosen(bool is_arbitrary) const;

  // Lottie illustrations are represented by their resource ID.
  std::optional<IllustrationPair<int>> lottie_illustrations() const {
    return lottie_illustrations_;
  }

  // If true, the sheet has a Google Password Manager banner at the top, which
  // is indented the same as the title and description.
  bool has_gpm_banner() const { return has_gpm_banner_; }

  std::optional<IllustrationPair<const gfx::VectorIcon&>> vector_illustrations()
      const {
    return vector_illustrations_;
  }

 protected:
  std::optional<IllustrationPair<int>> lottie_illustrations_;
  std::optional<IllustrationPair<const gfx::VectorIcon&>> vector_illustrations_;
  bool has_gpm_banner_ = false;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_MODEL_H_
