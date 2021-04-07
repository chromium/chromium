// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_

#include "base/memory/checked_ptr.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace autofill_assistant {

// Starts autofill-assistant flows. Uses a platform delegate to show UI and
// access platform-dependent features.
class Starter : public content::WebContentsObserver {
 public:
  explicit Starter(content::WebContents* web_contents,
                   StarterPlatformDelegate* platform_delegate);
  ~Starter() override;
  Starter(const Starter&) = delete;
  Starter& operator=(const Starter&) = delete;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Notification invoked by platform delegates to inform the tab helper that
  // relevant settings have changed.
  void OnSettingsChanged(bool proactive_help_setting_enabled,
                         bool msbb_setting_enabled);

 private:
  CheckedPtr<StarterPlatformDelegate> platform_delegate_;
  bool fetch_trigger_scripts_on_navigation_ = false;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_
