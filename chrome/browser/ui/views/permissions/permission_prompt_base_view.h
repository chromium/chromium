// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/browser/ui/url_identity.h"
#include "components/permissions/permission_prompt.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/media_preview/permission_prompt_previews_coordinator.h"
#endif

namespace content {
class WebContents;
}

class BrowserWindowInterface;

// Base view that provides security-related and shared functionality to
// permission prompts. This class will:
// * Compute a URL identity and provide it to subclasses
// * Elide the title as needed if it would be too long
// * Filter unintended button presses
// * Ensure no button is selected by default to prevent unintended button
// presses
// * Manage live camera and microphone previews when applicable
class PermissionPromptBaseView : public views::BubbleDialogDelegateView,
                                 public PictureInPictureOcclusionObserver,
                                 public content::WebContentsObserver {
  METADATA_HEADER(PermissionPromptBaseView, views::BubbleDialogDelegateView)

 public:
  PermissionPromptBaseView(
      content::WebContents* web_contents,
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

#if !BUILDFLAG(IS_CHROMEOS)
  const std::optional<PermissionPromptPreviewsCoordinator>&
  GetMediaPreviewsForTesting() const {
    return media_previews_;
  }
#endif

 protected:
  // Adds Media (Camera / Mic) live preview feeds at `index` if the current
  // prompt requests camera or microphone permissions. When
  // `allow_empty_device_ids` is false, at least one request must specify
  // non-empty requested audio/video capture device IDs.
  bool MaybeAddMediaPreview(size_t index, bool allow_empty_device_ids = false);

  // Updates the preferred device ranking in user prefs based on the user's
  // device selection in the active media preview, if any.
  void UpdateMediaDevicePreferenceRanking();

  // Tears down any active media preview.
  void ResetMediaPreview();
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
      content::WebContents* web_contents,
      permissions::PermissionPrompt::Delegate& delegate);

  static std::u16string GetAllowAlwaysText(
      const std::vector<std::unique_ptr<permissions::PermissionRequest>>&
          visible_requests);

  static std::u16string GetAllowAlwaysText(
      const std::vector<permissions::PermissionRequest*>& visible_requests);

  static std::u16string GetBlockText(
      const std::vector<std::unique_ptr<permissions::PermissionRequest>>&
          visible_requests);

  static std::u16string GetBlockText(
      const std::vector<permissions::PermissionRequest*>& visible_requests);

  // Starts observing our widget for occlusion by a picture-in-picture window.
  // Subclasses must manually call this if they override `AddedToWidget()`
  // without calling `PermissionPromptBaseView::AddedToWidget()`.
  void StartTrackingPictureInPictureOcclusion();

  void AnchorToPageInfoOrChip();

  BrowserWindowInterface* GetBrowser();
  const BrowserWindowInterface* GetBrowser() const;

  // Gets the permission prompt's top-level NativeWindow.
  gfx::NativeWindow GetNativeWindow();

  bool record_host_always_active_value() const {
    return record_host_always_active_value_;
  }

  permissions::RequestTypeForUma request_type() const { return request_type_; }

  std::vector<std::pair<size_t, size_t>> GetTitleBoldedRanges();
  void SetTitleBoldedRanges(
      std::vector<std::pair<size_t, size_t>> bolded_ranges);

 private:
  void HostPaintAsActiveChanged();

  // True if this permission prompt is for a picture-in-picture window. This
  // means it will be in an always-on-top window, and needs to be tracked by the
  // PictureInPictureOcclusionTracker.
  bool IsForPictureInPictureWindow() const;

  base::CallbackListSubscription host_paint_as_active_subscription_;

  const UrlIdentity url_identity_;

  ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};
  bool occluded_by_picture_in_picture_ = false;

  // Boolean value to track if the host was always active while the prompt was
  // displayed.
  bool record_host_always_active_value_ = true;

  // $ORIGIN in the title should be bolded, the ranges of the $ORIGINs are
  // gained while building the title string via `l10n_util::GetStringFUTF16()`.
  std::vector<std::pair<size_t, size_t>> title_bolded_ranges_ = {};

  permissions::RequestTypeForUma request_type_;

  base::WeakPtr<permissions::PermissionPrompt::Delegate>
      permission_prompt_delegate_;
#if !BUILDFLAG(IS_CHROMEOS)
  std::optional<PermissionPromptPreviewsCoordinator> media_previews_;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_
