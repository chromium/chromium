// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/test_help_bubble.h"

#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestHelpBubble)
DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestHelpBubbleFactory)

TestHelpBubble::TestHelpBubble(ui::ElementContext context,
                               HelpBubbleParams params)
    : context_(context), params_(std::move(params)) {}

TestHelpBubble::~TestHelpBubble() {
  // Needs to be called here while we still have access to derived class
  // methods.
  Close();
}

bool TestHelpBubble::ToggleFocusForAccessibility() {
  ++focus_count_;
  return true;
}

// Simulates the user dismissing the bubble.
void TestHelpBubble::SimulateDismiss() {
  std::move(params_.dismiss_callback).Run();
  Close();
}

// Simulates the bubble timing out.
void TestHelpBubble::SimulateTimeout() {
  std::move(params_.timeout_callback).Run();
  Close();
}

// Simualtes the user pressing one of the bubble buttons.
void TestHelpBubble::SimulateButtonPress(int button_index) {
  CHECK_LT(button_index, static_cast<int>(params_.buttons.size()));
  std::move(params_.buttons[button_index].callback).Run();
}

void TestHelpBubble::CloseBubbleImpl() {
  context_ = ui::ElementContext();
}

ui::ElementContext TestHelpBubble::GetContext() const {
  return context_;
}

std::unique_ptr<HelpBubble> TestHelpBubbleFactory::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  return std::make_unique<TestHelpBubble>(element->context(),
                                          std::move(params));
}

bool TestHelpBubbleFactory::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<ui::test::TestElement>();
}
