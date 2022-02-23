// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TEST_HELP_BUBBLE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TEST_HELP_BUBBLE_H_

#include <memory>

#include "base/auto_reset.h"
#include "chrome/browser/ui/user_education/help_bubble.h"
#include "chrome/browser/ui/user_education/help_bubble_factory.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {
class TrackedElement;
}

class TestHelpBubble : public HelpBubble {
 public:
  TestHelpBubble(ui::ElementContext context, HelpBubbleParams params);
  ~TestHelpBubble() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  const HelpBubbleParams& params() const { return params_; }

  // Simulates the user dismissing the bubble.
  void SimulateDismiss();

  // Simulates the bubble timing out.
  void SimulateTimeout();

  // Simualtes the user pressing one of the bubble buttons.
  void SimulateButtonPress(int button_index);

  // Get the number of times this bubble has has ToggleFocusForAccessibility()
  // called.
  int focus_count() const { return focus_count_; }

 protected:
  // HelpBubble:
  bool ToggleFocusForAccessibility() override;
  void CloseBubbleImpl() override;
  ui::ElementContext GetContext() const override;

 private:
  ui::ElementContext context_;
  HelpBubbleParams params_;
  int focus_count_ = 0;
};

class TestHelpBubbleFactory : public HelpBubbleFactory {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;

  std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                           HelpBubbleParams params) override;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TEST_HELP_BUBBLE_H_
