// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TOOLBAR_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TOOLBAR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kProfilePickerToolbarDontSignInButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kProfilePickerToolbarEffectsControlButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kProfilePickerToolbarStartBrowsingButtonElementId);

// Class responsible for the top toolbar shown within the First Run and Profile
// Creation flows.
//
// This toolbar hovers over the WebView and WebUI(s) rendered in the Profile
// Picker window. Adding a visual change (like a new button) to the toolbar
// should be carefully reviewed and tested to ensure it doesn't negatively
// impact the user experience (e.g. clashing with the WebView / WebUI contents).
class ProfilePickerToolbar : public views::View {
  METADATA_HEADER(ProfilePickerToolbar, views::View)

 public:
  class Builder {
   public:
    // Creates a new toolbar builder.
    //
    // `on_back_callback` is called when the back button is clicked. The toolbar
    // is always created with a back button.
    explicit Builder(base::RepeatingClosure on_back_callback);

    ~Builder();

    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    Builder(Builder&&);
    Builder& operator=(Builder&&);

    // Adds the "Don't sign in" button to the toolbar.
    Builder& WithDontSignInButton(
        base::RepeatingClosure on_dont_sign_in_callback);

    // Adds the "Start browsing" button to the toolbar.
    Builder& WithStartBrowsingButton(
        base::RepeatingClosure on_start_browsing_callback);

    // Adds the effects (audio/animations) control button to the toolbar.
    Builder& WithEffectsControlButton(
        base::RepeatingCallback<void(bool)> on_effects_control_callback,
        bool visible_by_default);

    // Builds a new `ProfilePickerToolbar`.
    //
    // All buttons are hidden by default, and their visibility is controlled by
    // the caller.
    std::unique_ptr<ProfilePickerToolbar> Build();

   private:
    base::RepeatingClosure on_back_callback_;
    base::RepeatingClosure on_dont_sign_in_callback_;
    base::RepeatingClosure on_start_browsing_callback_;
    bool effects_control_button_visible_by_default_ = false;
    base::RepeatingCallback<void(bool)> on_effects_control_callback_;
  };

  ~ProfilePickerToolbar() override;
  ProfilePickerToolbar(const ProfilePickerToolbar&) = delete;
  ProfilePickerToolbar& operator=(const ProfilePickerToolbar&) = delete;

  // Changes the visibility of the sign-in buttons (buttons that are related to
  // the sign-in flow), i.e. back and "Don't sign in" (if created).
  void SetSigninButtonsVisible(bool visible);

  // Returns whether any of the sign-in buttons (back or "Don't sign in") is
  // visible.
  bool AreSigninButtonsVisibleForTesting() const;

  // Changes the visibility of the "Don't sign in" button. It's no-op if the
  // button is not created.
  void SetDontSignInButtonVisible(bool visible);

  // Changes the visibility of the "Start browsing" button. It's no-op if the
  // button is not created.
  void SetStartBrowsingButtonVisible(bool visible);

  // Changes the visibility of the effects control button. It's no-op if the
  // button is not created.
  void SetEffectsControlButtonVisible(bool visible);

  // Returns whether effects (animations/audio) are enabled.
  bool AreEffectsEnabled() const;

 private:
  ProfilePickerToolbar();

  // Changes the visibility of the back button.
  void SetBackButtonVisible(bool visible);

  void AddSpacer();
  void AddBackButton(base::RepeatingClosure on_back_callback);
  void AddDontSignInButton(base::RepeatingClosure on_dont_sign_in_callback,
                           bool paint_border);
  void AddStartBrowsingButton(
      base::RepeatingClosure on_start_browsing_callback);
  void AddSeparator();
  void MaybeUpdateSeparatorVisibility();
  void AddEffectsControlButton(
      base::RepeatingCallback<void(bool)> on_effects_control_callback,
      bool visible_by_default);

  raw_ptr<views::View> sign_in_back_button_ = nullptr;
  raw_ptr<views::View> dont_sign_in_button_ = nullptr;
  raw_ptr<views::View> start_browsing_button_ = nullptr;
  raw_ptr<views::View> separator_ = nullptr;
  raw_ptr<views::View> effects_control_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TOOLBAR_H_
