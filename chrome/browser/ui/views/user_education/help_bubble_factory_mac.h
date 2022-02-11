// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_HELP_BUBBLE_FACTORY_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_HELP_BUBBLE_FACTORY_MAC_H_

#include "chrome/browser/ui/user_education/help_bubble_factory.h"

// Factory implementation for HelpBubbleViews.
class HelpBubbleFactoryMac : public HelpBubbleFactory {
 public:
  HelpBubbleFactoryMac();
  ~HelpBubbleFactoryMac() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubbleFactory:
  std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                           HelpBubbleParams params) override;
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;
};

#endif  // #ifndef
        // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_HELP_BUBBLE_FACTORY_MAC_H_
