// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_WEBUI_FLOATING_WEBUI_HELP_BUBBLE_FACTORY_H_
#define COMPONENTS_USER_EDUCATION_WEBUI_FLOATING_WEBUI_HELP_BUBBLE_FACTORY_H_

#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/webui/help_bubble_webui.h"

namespace user_education {

// Help bubble factory that anchors a floating Views help bubble to a WebUI.
//
// This factory is designed for help bubbles attached to non-tab WebUI (such as
// the WebUI toolbar or side panels); prefer `HelpBubbleFactoryWebUI` for help
// bubbles in WebUI that are displayed in tabs (that factory shows an embedded
// help bubble inside the WebUI DOM itself).
//
// Unlike `HelpBubbleFactoryWebUI`, which locks its anchor's visibility so that
// backgrounding a browser tab does not close the embedded bubble, floating help
// bubbles created by this factory do NOT lock `TrackedElementWebUI` visibility:
// if the hosting `WebView` / `WebContents` is hidden (e.g. when the toolbar is
// hidden in fullscreen mode), the anchor element reports hidden and the
// floating Views bubble closes.
class FloatingWebUIHelpBubbleFactory : public HelpBubbleFactoryViews {
 public:
  explicit FloatingWebUIHelpBubbleFactory(const HelpBubbleDelegate* delegate);
  ~FloatingWebUIHelpBubbleFactory() override;

  DECLARE_SAFE_CAST_TARGET()

  // HelpBubbleFactory:
  std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                           HelpBubbleParams params) override;
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_WEBUI_FLOATING_WEBUI_HELP_BUBBLE_FACTORY_H_
