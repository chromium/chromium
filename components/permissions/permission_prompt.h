// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_ui_selector.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace ui {
class Event;
}  // namespace ui

namespace permissions {
enum class PermissionPromptDisposition;

class PermissionRequest;

// This class is the platform-independent interface through which the permission
// request managers (which are one per tab) communicate to the UI surface.
// When the visible tab changes, the UI code must provide an object of this type
// to the manager for the visible tab.
class PermissionPrompt {
 public:
  // Permission prompt behavior on tab switching.
  enum TabSwitchingBehavior {
    // The prompt should be kept as-is on tab switching (usually because it's
    // part of the containing tab so it will be hidden automatically when
    // switching from said tab)
    kKeepPromptAlive,
    // Destroy the prompt but keep the permission request pending. When the user
    // revisits the tab, the permission prompt is re-displayed.
    kDestroyPromptButKeepRequestPending,
    // Destroy the prompt and treat the permission request as being resolved
    // with the PermissionAction::IGNORED result.
    kDestroyPromptAndIgnoreRequest,
  };

  // The delegate will receive events caused by user action which need to
  // be persisted in the per-tab UI state.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // These pointers should not be stored as the actual request objects may be
    // deleted upon navigation and so on.
    virtual const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
    Requests() = 0;

    // Get the single origin for the current set of requests.
    virtual GURL GetRequestingOrigin() const = 0;

    // Get the top-level origin currently displayed in the address bar
    // associated with the requests.
    virtual GURL GetEmbeddingOrigin() const = 0;

    virtual void Accept() = 0;
    virtual void AcceptThisTime() = 0;
    virtual void Deny() = 0;
    virtual void Dismiss() = 0;
    virtual void Ignore() = 0;

    // Called to explicitly finalize the request, if
    // |ShouldFinalizeRequestAfterDecided| returns false.
    virtual void FinalizeCurrentRequests() = 0;

    virtual void OpenHelpCenterLink(const ui::Event& event) = 0;

    // This method preemptively ignores a permission request but does not
    // finalize a permission prompt. That is needed in case a permission prompt
    // is a quiet chip. This should be called only if an origin is subscribed to
    // the `PermissionStatus.onchange` listener.
    virtual void PreIgnoreQuietPrompt() = 0;

    // If |ShouldCurrentRequestUseQuietUI| return true, this will provide a
    // reason as to why the quiet UI needs to be used. Returns `std::nullopt`
    // otherwise.
    virtual std::optional<PermissionUiSelector::QuietUiReason>
    ReasonForUsingQuietUi() const = 0;

    // Notification permission requests might use a quiet UI when the
    // "quiet-notification-prompts" feature is enabled. This is done either
    // directly by the user in notifications settings, or via automatic logic
    // that might trigger the current request to use the quiet UI.
    virtual bool ShouldCurrentRequestUseQuietUI() const = 0;

    // If the LocationBar is not visible, there is no place to display a quiet
    // permission prompt. Abusive prompts will be ignored.
    virtual bool ShouldDropCurrentRequestIfCannotShowQuietly() const = 0;

    // Whether the current request has been shown to the user at least once.
    virtual bool WasCurrentRequestAlreadyDisplayed() = 0;

    // Set whether the current request should be dismissed if the current tab is
    // closed.
    virtual void SetDismissOnTabClose() = 0;

    // Set whether the permission prompt bubble was shown for the current
    // request.
    virtual void SetPromptShown() = 0;

    // Set when the user made any decision for the currentrequest.
    virtual void SetDecisionTime() = 0;

    // Set when the user made any decision for manage settings.
    virtual void SetManageClicked() = 0;

    // Set when the user made any decision for clicking on learn more link.
    virtual void SetLearnMoreClicked() = 0;

    // HaTS surveys may display at an inconvenient time, such as when a chip
    // shown collapses after a certain timeout. To prevent affecting
    // usability, this setter sets a callback that is be called when a HaTS
    // survey is triggered to take appropriate actions.
    virtual void SetHatsShownCallback(base::OnceCallback<void()> callback) = 0;

    virtual content::WebContents* GetAssociatedWebContents() = 0;

    virtual base::WeakPtr<Delegate> GetWeakPtr() = 0;

    // Recreate the UI view because the UI flavor needs to change. Returns true
    // iff successful.
    virtual bool RecreateView() = 0;
  };

  typedef base::RepeatingCallback<
      std::unique_ptr<PermissionPrompt>(content::WebContents*, Delegate*)>
      Factory;

  // Create and display a platform specific prompt.
  static std::unique_ptr<PermissionPrompt> Create(
      content::WebContents* web_contents,
      Delegate* delegate);
  virtual ~PermissionPrompt() = default;

  // Updates where the prompt should be anchored. ex: fullscreen toggle.
  // Returns true, if the update was successful, and false if the caller should
  // recreate the view instead.
  virtual bool UpdateAnchor() = 0;

  // Get the behavior of this prompt when the user switches away from the
  // associated tab.
  virtual TabSwitchingBehavior GetTabSwitchingBehavior() = 0;

  // Get the type of prompt UI shown for metrics.
  virtual PermissionPromptDisposition GetPromptDisposition() const = 0;

  // Check if the view shown is an "Ask" prompt for metrics. Currently this only
  // distinguishes different prompt views displayed through the Page Embedded
  // Permission Element.
  virtual bool IsAskPrompt() const = 0;

  // Get the prompt view bounds in screen coordinates.
  virtual std::optional<gfx::Rect> GetViewBoundsInScreen() const = 0;

  // Get whether the permission request is allowed to be finalized as soon a
  // decision is transmitted. If this returns `false` the delegate should wait
  // for an explicit |Delegate::FinalizeCurrentRequests()| call to be made.
  virtual bool ShouldFinalizeRequestAfterDecided() const = 0;

  // Return what variant of the secondary UI is shown for Page Embedded
  // Permission Element.
  virtual std::vector<permissions::ElementAnchoredBubbleVariant>
  GetPromptVariants() const = 0;

  virtual std::optional<feature_params::PermissionElementPromptPosition>
  GetPromptPosition() const = 0;
};
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_H_
