// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"

class GURL;
namespace content {
class WebContents;
}  // namespace content

// A bubble that is shown when an automated password change
// run is stopped before it has completed. It attaches to the browser
// containing `web_contents` and is only shown when the `web_contents`
// is visible. It closes  if the tab containing the web_content is removed
// from the browser.
class AssistantStoppedBubbleCoordinator {
 public:
  static std::unique_ptr<AssistantStoppedBubbleCoordinator> Create(
      content::WebContents* web_contents,
      const GURL& url,
      const std::string& username);

  virtual ~AssistantStoppedBubbleCoordinator() = default;

  // Shows the assistant stopped bubble. This call has no effect
  // in the case where the `web_contents` is no longer
  // attached to a browser.
  virtual void Show() = 0;
  // Hides the bubble, unlike `Close` a hidden bubble can be
  // shown again by calling `Show`.
  virtual void Hide() = 0;
  // Closes the bubble, effectively destroying it.
  virtual void Close() = 0;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_H_
