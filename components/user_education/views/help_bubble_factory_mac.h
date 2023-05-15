// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_MAC_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_MAC_H_

#include "base/memory/raw_ptr.h"
#include "components/user_education/common/help_bubble_factory.h"

namespace user_education {

class HelpBubbleDelegate;

// Factory implementation for HelpBubbleViews.
class HelpBubbleFactoryMac : public HelpBubbleFactory {
 public:
  explicit HelpBubbleFactoryMac(const HelpBubbleDelegate* delegate);
  ~HelpBubbleFactoryMac() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubbleFactory:
  std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                           HelpBubbleParams params) override;
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;

 private:
  raw_ptr<const HelpBubbleDelegate> delegate_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_MAC_H_
