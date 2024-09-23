// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class AnimatedImageView;
class Label;
}  // namespace views

class AuthenticatorRequestSheetModel;
class NonAccessibleImageView;

// Defines the basic structure of sheets shown in the authenticator request
// dialog. Each sheet corresponds to a given step of the authentication flow,
// and encapsulates the controls above the Ok/Cancel buttons, namely:
//  -- an optional progress-bar-style activity indicator (at the top),
//  -- an optional `back icon`,
//  -- a pretty illustration in the top half of the dialog,
//  -- the title of the current step,
//  -- the description of the current step,
//  -- an optional view with step-specific content, added by subclasses, filling
//     the rest of the space, and
//  -- an optional contextual error.
//
// +-------------------------------------------------+
// |*************************************************|
// |. (<-). . . . . . . . . . . . . . . . . . . . . .|
// |. . . . I L L U S T R A T I O N   H E R E . . . .|
// |. . . . . . . . . . . . . . . . . . . . . . . . .|
// |                                                 |
// | +---------------------------------------------+ |
// | |     optional Step-specific header view      | |
// | +---------------------------------------------+ |
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
// |  optional contextual error                      |
// +-------------------------------------------------+
// |                                   OK   CANCEL   | <- Not part of this view.
// +-------------------------------------------------+
//
// TODO(crbug.com/41394359): The Web Authentication and Web Payment APIs
// both use the concept of showing multiple "sheets" in a single dialog. To
// avoid code duplication, consider factoring out common parts.
class AuthenticatorRequestSheetView : public views::View {
  METADATA_HEADER(AuthenticatorRequestSheetView, views::View)

 public:
  explicit AuthenticatorRequestSheetView(
      std::unique_ptr<AuthenticatorRequestSheetModel> model);
  AuthenticatorRequestSheetView(const AuthenticatorRequestSheetView&) = delete;
  AuthenticatorRequestSheetView& operator=(
      const AuthenticatorRequestSheetView&) = delete;
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
  // AutoFocus is a named boolean that indicates whether step-specific content
  // should automatically get focus when displayed.
  enum class AutoFocus {
    kNo,
    kYes,
  };

  // Returns the step specific header view. The header will be below the
  // illustration of the model and the title.
  virtual std::unique_ptr<views::View> BuildStepSpecificHeader();

  // Returns the step-specific content view the derived sheet wishes to provide,
  // if any, and whether that content should be initially focused.
  virtual std::pair<std::unique_ptr<views::View>, AutoFocus>
  BuildStepSpecificContent();

  // Returns the spacing between the step title and the description.
  virtual int GetSpacingBetweenTitleAndDescription();

 private:
  // Children of these views are removed by `ReInitChildViews`. To avoid
  // dangling pointers, group references to the children in a struct that is
  // easy to clear.
  struct ChildViews {
    raw_ptr<views::View> step_specific_header_ = nullptr;
    raw_ptr<views::View> step_specific_content_ = nullptr;
    raw_ptr<NonAccessibleImageView> step_illustration_image_ = nullptr;
    raw_ptr<views::AnimatedImageView> step_illustration_animation_ = nullptr;
    raw_ptr<views::Label> error_label_ = nullptr;
    raw_ptr<views::Label> title_label_ = nullptr;
    raw_ptr<views::Label> hint_label_ = nullptr;
  };

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
  ChildViews child_views_;
  AutoFocus should_focus_step_specific_content_ = AutoFocus::kNo;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_
