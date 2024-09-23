// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_CHROMEOS_IMPL_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_CHROMEOS_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/native_window_tracker.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chromeos/components/sharesheet/constants.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Button;
}  // namespace views

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace sharesheet {
class SharesheetService;
}  // namespace sharesheet
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace sharing_hub {

class SharingHubBubbleView;

// Controller component of the omnibox entry point for the Sharesheet dialog.
// Responsible for showing and hiding the Sharesheet.
class SharingHubBubbleControllerChromeOsImpl final
    : public SharingHubBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          SharingHubBubbleControllerChromeOsImpl> {
 public:
  SharingHubBubbleControllerChromeOsImpl(
      const SharingHubBubbleControllerChromeOsImpl&) = delete;
  SharingHubBubbleControllerChromeOsImpl& operator=(
      const SharingHubBubbleControllerChromeOsImpl&) = delete;

  ~SharingHubBubbleControllerChromeOsImpl() override;

  // SharingHubBubbleController:
  void HideBubble() override;
  void ShowBubble(share::ShareAttempt attempt) override;
  SharingHubBubbleView* sharing_hub_bubble_view() const override;
  bool ShouldOfferOmniboxIcon() override;

  // Returns the current profile.
  Profile* GetProfile() const;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

 protected:
  explicit SharingHubBubbleControllerChromeOsImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      SharingHubBubbleControllerChromeOsImpl>;

  void ShowSharesheet(views::Button* highlighted_button);
  void CloseSharesheet();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  sharesheet::SharesheetService* GetSharesheetService();
  void ShowSharesheetAsh();
  void CloseSharesheetAsh();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void ShowSharesheetLacros();
  void CloseSharesheetLacros();
  void OnSharesheetClosedLacros();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  void OnSharesheetClosed(views::Widget::ClosedReason reason);

  void DeselectIcon();

  views::ViewTracker highlighted_button_tracker_;
  gfx::NativeWindow parent_window_ = gfx::NativeWindow();
  std::unique_ptr<views::NativeWindowTracker> parent_window_tracker_ = nullptr;
  bool bubble_showing_ = false;
  base::WeakPtrFactory<SharingHubBubbleControllerChromeOsImpl>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_CHROMEOS_IMPL_H_
