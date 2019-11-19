// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/media/router/presentation/independent_otr_profile_manager.h"
#include "chrome/browser/media/router/presentation/presentation_navigation_policy.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;
class PresentationReceiverWindow;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

// This class controls a window which is used to render a receiver page for the
// Presentation API.  It handles creating the window to show the desired URL and
// showing it in fullscreen on the desired display.  This class should not be
// destroyed until |termination_callback| has been called, which may be called
// before Terminate() is ever called.
class PresentationReceiverWindowController final
    : public PresentationReceiverWindowDelegate,
      public content::WebContentsObserver,
      public content::WebContentsDelegate,
      public media_router::WiredDisplayPresentationReceiver {
 public:
  using TitleChangeCallback = base::RepeatingCallback<void(const std::string&)>;

  static std::unique_ptr<PresentationReceiverWindowController>
  CreateFromOriginalProfile(Profile* profile,
                            const gfx::Rect& bounds,
                            base::OnceClosure termination_callback,
                            TitleChangeCallback title_change_callback);

  ~PresentationReceiverWindowController() final;

  // WiredDisplayPresentationReceiver overrides.
  void Start(const std::string& presentation_id,
             const GURL& start_url) override;
  void Terminate() override;
  void ExitFullscreen() override;

  // PresentationReceiverWindowDelegate overrides.
  content::WebContents* web_contents() const final;

 private:
  friend class PresentationReceiverWindowControllerBrowserTest;

  PresentationReceiverWindowController(
      Profile* profile,
      const gfx::Rect& bounds,
      base::OnceClosure termination_callback,
      TitleChangeCallback title_change_callback);

  void OriginalProfileDestroyed(Profile* profile);

  // These methods are intended to be used by tests.
  void CloseWindowForTest();
  bool IsWindowActiveForTest() const;
  bool IsWindowFullscreenForTest() const;
  gfx::Rect GetWindowBoundsForTest() const;

  // PresentationReceiverWindowDelegate overrides.
  void WindowClosed() final;

  // content::WebContentsObserver overrides.
  void DidStartNavigation(content::NavigationHandle* handle) final;
  void TitleWasSet(content::NavigationEntry* entry) final;

  // content::WebContentsDelegate overrides.
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) final;
  void CloseContents(content::WebContents* source) final;
  bool ShouldSuppressDialogs(content::WebContents* source) final;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) final;
  bool ShouldFocusPageAfterCrash() final;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) final;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;

  // The profile used for the presentation.
  std::unique_ptr<IndependentOTRProfileManager::OTRProfileRegistration>
      otr_profile_registration_;

  // WebContents for rendering the receiver page.
  std::unique_ptr<content::WebContents> web_contents_;

  // The actual UI window for displaying the receiver page.
  PresentationReceiverWindow* window_;

  base::OnceClosure termination_callback_;

  // Gets called with the new title whenever TitleWasSet() is called.
  TitleChangeCallback title_change_callback_;

  media_router::PresentationNavigationPolicy navigation_policy_;

  DISALLOW_COPY_AND_ASSIGN(PresentationReceiverWindowController);
};

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_CONTROLLER_H_
