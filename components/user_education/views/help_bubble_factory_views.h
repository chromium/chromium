// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_factory.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/widget/widget.h"

namespace user_education {

class HelpBubbleDelegate;
class HelpBubbleEventRelay;

namespace internal {
struct HelpBubbleAnchorParams;
}

// Factory implementation for HelpBubbleViews.
class HelpBubbleFactoryViews : public HelpBubbleFactory {
 public:
  explicit HelpBubbleFactoryViews(const HelpBubbleDelegate* delegate);
  ~HelpBubbleFactoryViews() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubbleFactory:
  std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                           HelpBubbleParams params) override;
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;

 protected:
  std::unique_ptr<HelpBubble> CreateBubbleImpl(
      ui::TrackedElement* element,
      const internal::HelpBubbleAnchorParams& anchor,
      HelpBubbleParams params,
      std::unique_ptr<HelpBubbleEventRelay> event_relay);

 private:
  raw_ptr<const HelpBubbleDelegate> delegate_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_H_
