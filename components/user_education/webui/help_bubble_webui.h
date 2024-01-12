// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_WEBUI_HELP_BUBBLE_WEBUI_H_
#define COMPONENTS_USER_EDUCATION_WEBUI_HELP_BUBBLE_WEBUI_H_

#include <memory>

#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace content {
class WebContents;
}

namespace user_education {

class HelpBubbleHandlerBase;

// This is a thin wrapper around HelpBubbleHandler that implements the
// HelpBubble interaface.
class HelpBubbleWebUI : public HelpBubble {
 public:
  ~HelpBubbleWebUI() override;

  // Retrieves the `WebContents` that hosts this help bubble, if any, or null if
  // none. Will return null if the bubble is closed.
  content::WebContents* GetWebContents();

  // HelpBubble:
  bool ToggleFocusForAccessibility() override;
  gfx::Rect GetBoundsInScreen() const override;
  ui::ElementContext GetContext() const override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

 private:
  friend class HelpBubbleHandlerBase;

  HelpBubbleWebUI(HelpBubbleHandlerBase* handler,
                  ui::ElementIdentifier anchor_id);

  // HelpBubble:
  void CloseBubbleImpl() override;

  const raw_ptr<HelpBubbleHandlerBase> handler_;
  const ui::ElementIdentifier anchor_id_;
};

// This factory uses HelpBubbleHandler to show a help bubble and create a
// HelpBubbleWebUI wrapper.
class HelpBubbleFactoryWebUI : public HelpBubbleFactory {
 public:
  HelpBubbleFactoryWebUI();
  ~HelpBubbleFactoryWebUI() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubbleFactory:
  std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                           HelpBubbleParams params) override;
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_WEBUI_HELP_BUBBLE_WEBUI_H_
