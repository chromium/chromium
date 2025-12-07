// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEWS_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEWS_H_

#include <concepts>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace user_education {

// Views-specific implementation of the help bubble.
//
// Because this is a FrameworkSpecificImplementation, you can use:
//   help_bubble->AsA<HelpBubbleViews>()->bubble_view()
// to retrieve the underlying bubble view.
class HelpBubbleViews : public HelpBubble,
                        public views::WidgetObserver,
                        public ui::AcceleratorTarget {
 public:
  ~HelpBubbleViews() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubble:
  bool ToggleFocusForAccessibility() override;
  void OnAnchorBoundsChanged() override;
  gfx::Rect GetBoundsInScreen() const override;
  ui::ElementContext GetContext() const override;

  // ui::AcceleratorTarget
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // Retrieve the bubble view. If the bubble has been closed, this may return
  // null.
  auto* bubble_view_for_testing() { return help_bubble_view_.get(); }
  const auto* bubble_view_for_testing() const {
    return help_bubble_view_.get();
  }

  static views::BubbleBorder::Arrow TranslateArrow(HelpBubbleArrow arrow);

 protected:
  HelpBubbleViews(views::BubbleDialogDelegateView* help_bubble_view,
                  ui::TrackedElement* anchor_element);

  // HelpBubble:
  void CloseBubbleImpl() override;

 private:
  friend class HelpBubbleFactoryViews;
  friend class HelpBubbleFactoryMac;
  friend class HelpBubbleViewsTest;
  friend class HelpBubbleViewsCustomBubbleTest;

  // Clean up properties on the anchor view, if applicable.
  void MaybeResetAnchorView();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void OnElementHidden(ui::TrackedElement* element);
  void OnElementBoundsChanged(ui::TrackedElement* element);

  raw_ptr<views::BubbleDialogDelegateView> help_bubble_view_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_observation_{this};

  // Track the anchor element to determine if/when it goes away.
  raw_ptr<const ui::TrackedElement> anchor_element_ = nullptr;

  // Listens so that the bubble can be closed if the anchor element disappears.
  // The specific anchor view is not tracked because in a few cases (e.g. Mac
  // native menus) the anchor view is not the anchor element itself but a
  // placeholder.
  base::CallbackListSubscription anchor_hidden_subscription_;

  // Listens for changes to the anchor bounding rect that are independent of the
  // anchor view. Necessary for e.g. WebUI elements, which can be scrolled or
  // moved within the web page.
  base::CallbackListSubscription anchor_bounds_changed_subscription_;

  base::WeakPtrFactory<HelpBubbleViews> weak_ptr_factory_{this};
};

// Help bubble that wraps a custom help bubble view.
//
// Rather than using this directly, prefer
// `CreateCustomHelpBubbleViewFactoryCallback()` as it removes a significant
// amount of boilerplate.
class CustomHelpBubbleViews : public HelpBubbleViews, public CustomHelpBubble {
 public:
  using UserAction = CustomHelpBubbleUi::UserAction;

  // Create a help bubble from a custom Views-based help bubble dialog.
  // Prefer `CreateCustomHelpBubbleViewFactoryCallback()`.
  //
  // NOTE: this hooks the `Widget::MakeCloseSynchronous` method and you should
  // NOT call that yourself. ESC and (X) are always handled, and the dialog
  // accept and cancel buttons (if present) will map to `accept_button_action`
  // and `cancel_button_action` correspondingly if specified.
  template <typename T>
    requires(std::derived_from<T, views::BubbleDialogDelegateView> &&
             std::derived_from<T, CustomHelpBubbleUi>)
  CustomHelpBubbleViews(
      std::unique_ptr<views::Widget> widget,
      T* bubble,
      ui::TrackedElement* anchor_element,
      std::optional<UserAction> accept_button_action = std::nullopt,
      std::optional<UserAction> cancel_button_action = std::nullopt)
      : CustomHelpBubbleViews(std::move(widget),
                              bubble,
                              *bubble,
                              anchor_element,
                              accept_button_action,
                              cancel_button_action) {}

  ~CustomHelpBubbleViews() override;

 protected:
  CustomHelpBubbleViews(std::unique_ptr<views::Widget> widget,
                        views::BubbleDialogDelegateView* bubble,
                        CustomHelpBubbleUi& ui,
                        ui::TrackedElement* anchor_element,
                        std::optional<UserAction> accept_button_action,
                        std::optional<UserAction> cancel_button_action);

 private:
  void OnHelpBubbleClosing(views::Widget::ClosedReason closed_reason);

  std::unique_ptr<views::Widget> help_bubble_widget_;
  std::optional<UserAction> accept_button_action_;
  std::optional<UserAction> cancel_button_action_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEWS_H_
