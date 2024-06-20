// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_AUTOFILL_HELP_BUBBLE_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_AUTOFILL_HELP_BUBBLE_FACTORY_H_

#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "ui/base/interaction/element_tracker.h"

// Creates a help bubble attached to an autofill dialog (or other dialog that
// pops up over the content pane and disappears on most interactions).
//
// This help bubble responds to events differently so that it does not trigger
// the autofill bubble to disappear on interaction. The downside is that it
// cannot be directly focused. Therefore, bubbles with interactive buttons other
// than "close" are precluded.
class AutofillHelpBubbleFactory
    : public user_education::HelpBubbleFactoryViews {
 public:
  explicit AutofillHelpBubbleFactory(
      const user_education::HelpBubbleDelegate* delegate);
  ~AutofillHelpBubbleFactory() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubbleFactory:
  std::unique_ptr<user_education::HelpBubble> CreateBubble(
      ui::TrackedElement* element,
      user_education::HelpBubbleParams params) override;
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_AUTOFILL_HELP_BUBBLE_FACTORY_H_
