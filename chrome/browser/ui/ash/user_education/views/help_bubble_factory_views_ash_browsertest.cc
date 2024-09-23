// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/views/help_bubble_factory_views_ash.h"

#include <memory>
#include <optional>

#include "ash/user_education/user_education_class_properties.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_view_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace {

// Aliases.
using ::ash::HelpBubbleContext;
using ::ash::HelpBubbleViewAsh;
using ::ash::HelpBubbleViewsAsh;
using ::ash::kHelpBubbleContextKey;
using ::user_education::HelpBubbleParams;
using ::user_education::HelpBubbleView;
using ::user_education::HelpBubbleViews;

// Helpers ---------------------------------------------------------------------

HelpBubbleParams CreateHelpBubbleParams(ash::HelpBubbleId help_bubble_id) {
  HelpBubbleParams help_bubble_params;
  help_bubble_params.extended_properties =
      ash::user_education_util::CreateExtendedProperties(help_bubble_id);
  return help_bubble_params;
}

}  // namespace

// HelpBubbleFactoryViewsAshBrowserTest ----------------------------------------

// Base class for browser tests of `HelpBubbleFactoryViewsAsh`, parameterized
// by help bubble context.
class HelpBubbleFactoryViewsAshBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::optional<HelpBubbleContext>> {
 public:
  // Returns the help bubble context to use given test parameterization.
  std::optional<HelpBubbleContext> GetHelpBubbleContext() const {
    return GetParam();
  }

  // Returns the help bubble factory registry for the active browser profile.
  user_education::HelpBubbleFactoryRegistry& GetHelpBubbleFactoryRegistry() {
    return UserEducationServiceFactory::GetForBrowserContext(
               browser()->profile())
        ->help_bubble_factory_registry();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HelpBubbleFactoryViewsAshBrowserTest,
    testing::Values(std::make_optional(HelpBubbleContext::kDefault),
                    std::make_optional(HelpBubbleContext::kAsh),
                    std::nullopt));

// Tests -----------------------------------------------------------------------

// Verifies that an Ash-specific help bubble will only take precedence over a
// standard Views-specific help bubble if the tracked element's help bubble
// context is explicitly set to `ash::HelpBubbleContext::kAsh`.
IN_PROC_BROWSER_TEST_P(HelpBubbleFactoryViewsAshBrowserTest, CreateBubble) {
  // Create an anchor `view` with parameterized help bubble `context`.
  auto view = std::make_unique<views::View>();
  std::optional<HelpBubbleContext> context = GetHelpBubbleContext();
  if (context.has_value()) {
    view->SetProperty(kHelpBubbleContextKey, context.value());
  }

  // Show the anchor `view` in a `widget`.
  views::UniqueWidgetPtr widget(std::make_unique<views::Widget>());
  widget->Init(views::Widget::InitParams(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET));
  auto* view_ptr = widget->GetContentsView()->AddChildView(std::move(view));
  widget->ShowInactive();

  // Register the anchor `view` and cache the associated `element`.
  ui::TrackedElement* element =
      views::ElementTrackerViews::GetInstance()->GetElementForView(
          view_ptr, /*assign_temporary_id=*/true);
  ASSERT_TRUE(element);

  // Create a help `bubble` anchored to `element`.
  auto bubble = GetHelpBubbleFactoryRegistry().CreateHelpBubble(
      element, CreateHelpBubbleParams(ash::HelpBubbleId::kTest));
  ASSERT_TRUE(bubble);

  // The help `bubble` should be Ash-specific depending on `context`.
  bool is_ash_context = context == HelpBubbleContext::kAsh;
  ASSERT_EQ(bubble->IsA<HelpBubbleViewsAsh>(), is_ash_context);
  ASSERT_NE(bubble->IsA<HelpBubbleViews>(), is_ash_context);
}
