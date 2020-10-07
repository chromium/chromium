// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

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
    virtual ~Delegate() {}

    // These pointers should not be stored as the actual request objects may be
    // deleted upon navigation and so on.
    virtual const std::vector<PermissionRequest*>& Requests() = 0;

    // Get the top-level origin currently displayed in the address bar
    // associated with the requests.
    virtual GURL GetEmbeddingOrigin() const = 0;

    virtual void Accept() = 0;
    virtual void Deny() = 0;
    virtual void Closing() = 0;

    // Whether the current request has been shown to the user at least once.
    virtual bool WasCurrentRequestAlreadyDisplayed() = 0;
  };

  typedef base::Callback<
      std::unique_ptr<PermissionPrompt>(content::WebContents*, Delegate*)>
      Factory;

  // Create and display a platform specific prompt.
  static std::unique_ptr<PermissionPrompt> Create(
      content::WebContents* web_contents,
      Delegate* delegate);
  virtual ~PermissionPrompt() {}

  // Updates where the prompt should be anchored. ex: fullscreen toggle.
  virtual void UpdateAnchorPosition() = 0;

  // Get the behavior of this prompt when the user switches away from the
  // associated tab.
  virtual TabSwitchingBehavior GetTabSwitchingBehavior() = 0;

  // Get the type of prompt UI shown for metrics.
  virtual PermissionPromptDisposition GetPromptDisposition() const = 0;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_H_
