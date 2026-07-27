// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WEB_CONTENTS_DELEGATE_BROWSER_WEB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_WEB_CONTENTS_DELEGATE_BROWSER_WEB_CONTENTS_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class ExclusiveAccessManager;
class BrowserWindow;
class DesktopBrowserWindowCapabilities;

// This class handles the WebContentsDelegate responsibilities of its host
// browser.
class BrowserWebContentsDelegate : public content::WebContentsDelegate {
 public:
  DECLARE_USER_DATA(BrowserWebContentsDelegate);

  BrowserWebContentsDelegate(BrowserWindowInterface* browser,
                             ExclusiveAccessManager& exclusive_access_manager,
                             BrowserWindow& window,
                             DesktopBrowserWindowCapabilities& capabilities);
  BrowserWebContentsDelegate(const BrowserWebContentsDelegate&) = delete;
  BrowserWebContentsDelegate& operator=(const BrowserWebContentsDelegate&) =
      delete;
  ~BrowserWebContentsDelegate() override;

  static BrowserWebContentsDelegate* From(BrowserWindowInterface* browser);

  // content::WebContentsDelegate:
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void RequestPointerLock(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void LostPointerLock() override;
  bool IsWaitingForPointerLockPrompt(
      content::WebContents* web_contents) override;
  bool AllowKeyboardLockForInnerContents(
      content::WebContents* web_contents) override;
  void RequestKeyboardLock(content::WebContents* web_contents,
                           bool esc_key_locked) override;
  void CancelKeyboardLockRequest(content::WebContents* web_contents) override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  int GetTopControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize(
      content::WebContents* contents) override;
  int GetVirtualKeyboardHeight(content::WebContents* contents) override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  bool CanOverscrollContent() override;
  bool ShouldPreserveAbortedURLs(content::WebContents* source) override;
  void SetFocusToLocationBar() override;
  void PreHandleDragUpdate(const content::DropData& drop_data,
                           const gfx::PointF& client_pt) override;
  void PreHandleDragExit() override;
  void HandleDragEnded() override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  void CreateSmsPrompt(content::RenderFrameHost*,
                       const std::vector<url::Origin>&,
                       const std::string& one_time_code,
                       base::OnceClosure on_confirm,
                       base::OnceClosure on_cancel) override;
  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;
  void OnDidBlockNavigation(
      content::WebContents* web_contents,
      const GURL& blocked_url,
      const GURL& initiator_url,
      const url::Origin& initiator_origin,
      blink::mojom::NavigationBlockedReason reason) override;
  bool IsBackForwardCacheSupported(content::WebContents& web_contents) override;

 private:
  const raw_ref<ExclusiveAccessManager> exclusive_access_manager_;
  const raw_ref<BrowserWindow> window_;
  const raw_ref<DesktopBrowserWindowCapabilities> capabilities_;
  const raw_ref<BrowserWindowInterface> browser_;
  ui::ScopedUnownedUserData<BrowserWebContentsDelegate> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WEB_CONTENTS_DELEGATE_BROWSER_WEB_CONTENTS_DELEGATE_H_
