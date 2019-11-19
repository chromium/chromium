// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class AuthenticatorRequestSheetModel;
class NonAccessibleImageView;

// Defines the basic structure of sheets shown in the authenticator request
// dialog. Each sheet corresponds to a given step of the authentication flow,
// and encapsulates the controls above the Ok/Cancel buttons, namely:
//  -- an optional progress-bar-style activity indicator (at the top),
//  -- an optional `back icon`,
//  -- a pretty illustration in the top half of the dialog,
//  -- the title of the current step,
//  -- the description of the current step, and
//  -- an optional view with step-specific content, added by subclasses, filling
//     the rest of the space.
//
// +-------------------------------------------------+
// |*************************************************|
// |. (<-). . . . . . . . . . . . . . . . . . . . . .|
// |. . . . I L L U S T R A T I O N   H E R E . . . .|
// |. . . . . . . . . . . . . . . . . . . . . . . . .|
// |                                                 |
// | Title of the current step                       |
// |                                                 |
// | Description text explaining to the user what    |
// | this step is all about.                         |
// |                                                 |
// | +---------------------------------------------+ |
// | |                                             | |
// | |          Step-specific content view         | |
// | |                                             | |
// | |                                             | |
// | +---------------------------------------------+ |
// +-------------------------------------------------+
// |                                   OK   CANCEL   | <- Not part of this view.
// +-------------------------------------------------+
//
// TODO(https://crbug.com/852352): The Web Authentication and Web Payment APIs
// both use the concept of showing multiple "sheets" in a single dialog. To
// avoid code duplication, consider factoring out common parts.
class AuthenticatorRequestSheetView : public views::View,
                                      public views::ButtonListener {
 public:
  explicit AuthenticatorRequestSheetView(
      std::unique_ptr<AuthenticatorRequestSheetModel> model);
  ~AuthenticatorRequestSheetView() override;

  // Recreates the standard child views on this sheet, potentially including
  // step-specific content if any. This is called once after this SheetView is
  // constructed, and potentially multiple times afterwards when the SheetModel
  // changes.
  void ReInitChildViews();

  // Returns the control on this sheet that should initially have focus instead
  // of the OK/Cancel buttons on the dialog; or returns nullptr if the regular
  // dialog button should have focus.
  views::View* GetInitiallyFocusedView();

  AuthenticatorRequestSheetModel* model() { return model_.get(); }

 protected:
  // Returns the step-specific view the derived sheet wishes to provide, if any.
  virtual std::unique_ptr<views::View> BuildStepSpecificContent();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // Creates the upper half of the sheet, consisting of a pretty illustration
  // overlayed with absolutely positioned controls (the activity indicator and
  // the back button) rendered on top.
  std::unique_ptr<views::View> CreateIllustrationWithOverlays();

  // Creates the lower half of the sheet, consisting of the title, description,
  // and step-specific content, if any.
  std::unique_ptr<views::View> CreateContentsBelowIllustration();

  // Updates the illustration icon shown on the sheet.
  void UpdateIconImageFromModel();

  // views::View:
  void OnThemeChanged() override;

  std::unique_ptr<AuthenticatorRequestSheetModel> model_;
  views::Button* back_arrow_button_ = nullptr;
  views::View* step_specific_content_ = nullptr;
  NonAccessibleImageView* step_illustration_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestSheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_
