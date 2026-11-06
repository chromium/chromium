// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_CONTROLLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace default_browser {

// A handle for guidance that points a bubble at the "Set default" button of
// the system Settings window. `CompletionCallback` runs when the guidance ends
// on its own, and is not run if the controller is destroyed first.
class AnnotatedSetterBubbleController {
 public:
  enum class Outcome {
    kSettingsWindowClosed,
    kSettingsWindowNotFound,
    kButtonNotFound,
    kDismissed,
  };

  using CompletionCallback = base::OnceCallback<void(Outcome)>;
  using Factory =
      base::RepeatingCallback<std::unique_ptr<AnnotatedSetterBubbleController>(
          base::TimeDelta find_timeout,
          bool can_pin_to_taskbar,
          CompletionCallback on_complete)>;

  virtual ~AnnotatedSetterBubbleController() = default;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_CONTROLLER_H_
