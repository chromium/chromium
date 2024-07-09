// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_

#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/browser/ui/url_identity.h"
#include "components/permissions/permission_prompt.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

// Base view that provide security-related functionality to permission prompts.
// This class will:
// * Compute an URL identity and provide it to subclasses
// * Elide the title as needed if it would be too long
// * Filter unintended button presses
// * Ensure no button is selected by default to prevent unintended button
// presses
class PermissionPromptBaseView : public views::BubbleDialogDelegateView,
                                 public PictureInPictureOcclusionObserver {
  METADATA_HEADER(PermissionPromptBaseView, views::BubbleDialogDelegateView)

 public:
  PermissionPromptBaseView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate);
  ~PermissionPromptBaseView() override;

  // views::BubbleDialogDelegateView:
  // Overridden to elide the prompt title if needed
  void AddedToWidget() override;

  // Overridden to provide input protection on dialog default buttons.
  bool ShouldIgnoreButtonPressedEventHandling(
      View* button,
      const ui::Event& event) const override;

  // PictureInPictureOcclusionObserver:
  void OnOcclusionStateChanged(bool occluded) override;

 protected:
  // Performs clickjacking checks and executes the button callback if the click
  // is valid. Subclasses need to make sure to set this as the callback for
  // custom buttons in order for this to work. This function will call
  // |RunButtonCallback| if the checks pass.
  void FilterUnintenedEventsAndRunCallbacks(int button_view_id,
                                            const ui::Event& event);

  // Called if a button press event has passes the input protections checks.
  // Needs to be implemented.
  virtual void RunButtonCallback(int button_view_id) = 0;

  const UrlIdentity& GetUrlIdentityObject() const { return url_identity_; }

  static UrlIdentity GetUrlIdentity(
      Browser* browser,
      permissions::PermissionPrompt::Delegate& delegate);

  static std::u16string GetAllowAlwaysText(
      const std::vector<raw_ptr<permissions::PermissionRequest,
                                VectorExperimental>>& visible_requests);

  // Starts observing our widget for occlusion by a picture-in-picture window.
  // Subclasses must manually call this if they override `AddedToWidget()`
  // without calling `PermissionPromptBaseView::AddedToWidget()`.
  void StartTrackingPictureInPictureOcclusion();

  void AnchorToPageInfoOrChip();

  Browser* browser() const { return browser_; }

  std::vector<std::pair<size_t, size_t>> GetTitleBoldedRanges();
  void SetTitleBoldedRanges(
      std::vector<std::pair<size_t, size_t>> bolded_ranges);

 private:
  const UrlIdentity url_identity_;

  ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};
  bool occluded_by_picture_in_picture_ = false;

  // True if this permission prompt is for a picture-in-picture window. This
  // means it will be in an always-on-top window, and needs to be tracked by the
  // PictureInPictureOcclusionTracker.
  const bool is_for_picture_in_picture_window_;

  const raw_ptr<Browser> browser_ = nullptr;

  // $ORIGIN in the title should be bolded, the ranges of the $ORIGINs are
  // gained while building the title string via `l10n_util::GetStringFUTF16()`.
  std::vector<std::pair<size_t, size_t>> title_bolded_ranges_ = {};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_
