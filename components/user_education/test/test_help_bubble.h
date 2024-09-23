// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_TEST_HELP_BUBBLE_H_
#define COMPONENTS_USER_EDUCATION_TEST_TEST_HELP_BUBBLE_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace ui {
class TrackedElement;
}

namespace user_education::test {

class TestHelpBubbleElement;

class TestHelpBubble : public HelpBubble {
 public:
  static constexpr int kNoButtonWithTextIndex = -1;

  TestHelpBubble(ui::TrackedElement* element, HelpBubbleParams params);
  ~TestHelpBubble() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kElementId);
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  const HelpBubbleParams& params() const { return params_; }

  const ui::TrackedElement* anchor_element() const {
    return anchor_element_.get();
  }

  // Simulates the user dismissing the bubble.
  void SimulateDismiss();

  // Simulates the bubble timing out.
  void SimulateTimeout();

  // Simulates the user pressing one of the bubble buttons.
  void SimulateButtonPress(int button_index);

  // Provides the index of a button with a given string value as its text
  // property. If one does not exist, returns -1.
  int GetIndexOfButtonWithText(std::u16string text);

  // Get the number of times this bubble has has ToggleFocusForAccessibility()
  // called.
  int focus_count() const { return focus_count_; }

 protected:
  // HelpBubble:
  bool ToggleFocusForAccessibility() override;
  void CloseBubbleImpl() override;
  ui::ElementContext GetContext() const override;

 private:
  void OnElementHidden(ui::TrackedElement* element);

  std::unique_ptr<TestHelpBubbleElement> bubble_element_;
  raw_ptr<ui::TrackedElement> anchor_element_;
  base::CallbackListSubscription element_hidden_subscription_;
  HelpBubbleParams params_;
  int focus_count_ = 0;

  base::WeakPtrFactory<TestHelpBubble> weak_ptr_factory_{this};
};

class TestHelpBubbleElement : public ui::test::TestElementBase {
 public:
  TestHelpBubbleElement(base::WeakPtr<TestHelpBubble> bubble,
                        ui::ElementIdentifier identifier,
                        ui::ElementContext context);
  ~TestHelpBubbleElement() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  TestHelpBubble* bubble() { return bubble_.get(); }
  const TestHelpBubble* bubble() const { return bubble_.get(); }

 private:
  base::WeakPtr<TestHelpBubble> bubble_;
};

class TestHelpBubbleFactory : public HelpBubbleFactory {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;

  std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                           HelpBubbleParams params) override;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_TEST_HELP_BUBBLE_H_
