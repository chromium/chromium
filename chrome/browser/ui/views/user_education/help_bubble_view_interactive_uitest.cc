// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble_event_relay.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

using user_education::HelpBubbleArrow;
using user_education::HelpBubbleParams;
using user_education::HelpBubbleView;

namespace {

// It is very important to create a situation in which the transparent bubble
// and the help bubble are both inside the bounds of the browser, and preferably
// inside the contents view. This should be sufficient.
constexpr gfx::Rect kTestBubbleAnchorRect{10, 10, 10, 10};
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestBubbleElementId);

// A bubble that anchors to the top left of the contents view in a browser and
// which should be transparent to events/not activatable.
class TestBubbleView : public views::BubbleDialogDelegateView {
 public:
  explicit TestBubbleView(views::View* anchor_view)
      : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    SetPreferredSize(gfx::Size(100, 100));
    SetCanActivate(false);
    set_focus_traversable_from_anchor_view(false);
    SetProperty(views::kElementIdentifierKey, kTestBubbleElementId);
  }

  ~TestBubbleView() override = default;

  METADATA_HEADER(TestBubbleView, BubbleDialogDelegateView)

  // views::BubbleDialogDelegateView:
  gfx::Rect GetAnchorRect() const override {
    gfx::Rect rect = kTestBubbleAnchorRect;
    rect.Offset(GetAnchorView()->GetBoundsInScreen().OffsetFromOrigin());
    return rect;
  }
};

BEGIN_METADATA(TestBubbleView)
END_METADATA

class TestHelpBubbleFactory : public user_education::HelpBubbleFactoryViews {
 public:
  TestHelpBubbleFactory() : HelpBubbleFactoryViews(GetHelpBubbleDelegate()) {}
  ~TestHelpBubbleFactory() override = default;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // Returns whether the bubble owner can show a bubble for the TrackedElement.
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override {
    if (auto* const element_views =
            element->AsA<views::TrackedElementViews>()) {
      return views::IsViewClass<TestBubbleView>(element_views->view());
    }
    return false;
  }

  // Called to actually show the bubble.
  std::unique_ptr<user_education::HelpBubble> CreateBubble(
      ui::TrackedElement* element,
      HelpBubbleParams params) override {
    user_education::internal::HelpBubbleAnchorParams anchor;
    anchor.view = element->AsA<views::TrackedElementViews>()->view();
    auto* const target =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            ContentsWebView::kContentsWebViewElementId, element->context());
    return CreateBubbleImpl(
        element, anchor, std::move(params),
        CreateWindowHelpBubbleEventRelay(target->GetWidget()));
  }
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestHelpBubbleFactory)

}  // namespace

class HelpBubbleViewInteractiveUiTest : public InteractiveBrowserTest {
 public:
  HelpBubbleViewInteractiveUiTest() = default;
  ~HelpBubbleViewInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    factories_.MaybeRegister<TestHelpBubbleFactory>();
    factories_.MaybeRegister<user_education::HelpBubbleFactoryViews>(
        GetHelpBubbleDelegate());
  }

  void TearDownOnMainThread() override {
    help_bubbles_.clear();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  static HelpBubbleParams GetBubbleParams() {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;
    return params;
  }

  // Shows the help anchored to the view `anchor`, and waits for it to appear.
  auto ShowHelpBubble(ElementSpecifier anchor,
                      HelpBubbleParams params = GetBubbleParams()) {
    return Steps(
        WithElement(anchor,
                    [this, params = std::move(params)](
                        ui::TrackedElement* anchor) mutable {
                      help_bubbles_.emplace_back(factories().CreateHelpBubble(
                          anchor, std::move(params)));
                    }),
        std::move(WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting)
                      .SetTransitionOnlyOnEvent(true)));
  }

  // Closes the current help bubble and waits for it to hide.
  auto CloseHelpBubble() {
    return Steps(
        WithView(HelpBubbleView::kHelpBubbleElementIdForTesting,
                 [](HelpBubbleView* bubble) { bubble->GetWidget()->Close(); }),
        std::move(WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting)
                      .SetTransitionOnlyOnEvent(true)));
  }

  user_education::HelpBubbleFactoryRegistry& factories() { return factories_; }

 private:
  user_education::HelpBubbleFactoryRegistry factories_;
  std::vector<std::unique_ptr<user_education::HelpBubble>> help_bubbles_;
};

IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest,
                       WidgetNotActivatedByDefault) {
  RunTestSequence(
      // The browser should be the active window.
      CheckViewProperty(kBrowserViewElementId, &BrowserView::IsActive, true),
      // Focus the toolbar and verify that it is focused.
      WithView(kBrowserViewElementId,
               [](BrowserView* view) { view->FocusToolbar(); }),
      CheckView(
          kBrowserViewElementId,
          [](BrowserView* view) {
            return view->GetFocusManager()->GetFocusedView();
          },
          testing::Ne(nullptr)),
      // Show the help bubble on the app menu and verify that it appears as
      // expected.
      ShowHelpBubble(kToolbarAppMenuButtonElementId),
      CheckView(HelpBubbleView::kHelpBubbleElementIdForTesting,
                [](HelpBubbleView* bubble) {
                  return bubble->GetWidget()->IsVisible();
                }),
      // The browser should still be the active window.
      CheckViewProperty(kBrowserViewElementId, &BrowserView::IsActive, true),
      // The help bubble widget should not steal focus.
      CheckView(
          HelpBubbleView::kHelpBubbleElementIdForTesting,
          [](HelpBubbleView* view) { return view->GetWidget()->IsActive(); },
          false),
      // Close the bubble and clean up.
      CloseHelpBubble());
}

// This is a regression test to ensure that help bubbles prevent other bubbles
// they are anchored to from closing on loss of focus. Failing to do this
// results in situations where a user can abort a user education journey by
// entering accessible keyboard navigation commands to try to read the help
// bubble, or by trying to interact with the help bubble with the mouse to e.g.
// close it.
//
// There's a race condition on at least Linux where the focus update happens
// later than expected, on an OS message callback, which can kill the tab editor
// bubble while we're trying to reactivate it below
// (https://crbug.com/372283580).
#if BUILDFLAG(IS_LINUX)
#define MAYBE_BubblePreventsCloseOnLossOfFocus \
  DISABLED_BubblePreventsCloseOnLossOfFocus
#else
#define MAYBE_BubblePreventsCloseOnLossOfFocus BubblePreventsCloseOnLossOfFocus
#endif
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest,
                       MAYBE_BubblePreventsCloseOnLossOfFocus) {
  browser()->tab_strip_model()->AddToNewGroup({0});

  HelpBubbleParams params;
  params.body_text = u"foo";

  // gfx::NativeView help_bubble_native_view = gfx::NativeView();

  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kSkipTest,
          "Programmatic window activation doesn't work on all platforms."),
      ObserveState(views::test::kCurrentWidgetFocus),

      // Trigger the tab group editor.
      AfterShow(kTabGroupHeaderElementId,
                [](ui::TrackedElement* element) {
                  // Show the tab group editor bubble.
                  auto* const view = AsView(element);
                  view->ShowContextMenu(
                      view->GetLocalBounds().CenterPoint(),
                      ui::MenuSourceType::MENU_SOURCE_KEYBOARD);
                }),
      WaitForShow(kTabGroupEditorBubbleId),

      // Display a help bubble attached to the tab group editor.
      ShowHelpBubble(kTabGroupEditorBubbleId, std::move(params)),

      // Activate the help bubble. This should not cause the editor to close.
      ActivateSurface(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(kTabGroupEditorBubbleId),

      // Close the help bubble.
      CloseHelpBubble(),

      // Re-Activate the dialog. It may or may not receive activation when the
      // help bubble closes.
      ActivateSurface(kTabGroupEditorBubbleId),

      // Now that the help bubble is gone, locate the editor again and transfer
      // activation to its primary window widget (the browser window) - this
      // should close the editor as it is no longer pinned by the help bubble.
      ActivateSurface(kToolbarAppMenuButtonElementId),

      // Verify that the editor bubble closes now that it has lost focus.
      WaitForHide(kTabGroupEditorBubbleId));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest,
                       ElementIdentifierFindsButton) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, default_button_clicked);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, non_default_button_clicked);
  constexpr char16_t kButton1Text[] = u"button 1";
  constexpr char16_t kButton2Text[] = u"button 2";

  user_education::HelpBubbleParams params = GetBubbleParams();

  user_education::HelpBubbleButtonParams button1;
  button1.text = kButton1Text;
  button1.is_default = true;
  button1.callback = default_button_clicked.Get();
  params.buttons.emplace_back(std::move(button1));

  user_education::HelpBubbleButtonParams button2;
  button2.text = kButton2Text;
  button2.is_default = false;
  button2.callback = non_default_button_clicked.Get();
  params.buttons.emplace_back(std::move(button2));

  EXPECT_CALL(default_button_clicked, Run).Times(1);

  RunTestSequence(
      // Show a help bubble and verify the button text.
      ShowHelpBubble(kToolbarAppMenuButtonElementId, std::move(params)),
      CheckViewProperty(HelpBubbleView::kDefaultButtonIdForTesting,
                        &views::LabelButton::GetText, kButton1Text),
      CheckViewProperty(HelpBubbleView::kFirstNonDefaultButtonIdForTesting,
                        &views::LabelButton::GetText, kButton2Text),
      // Press the default button; the bubble should close.
      PressButton(HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting));
}

namespace {

constexpr char kLinuxWaylandErrorMessage[] =
    "Because of the way events are routed and bounds are reported on Wayland "
    "this test isn't reliable. It has been manually tested, and based on the "
    "way the annotation event routing works, if it did not work (a) it would "
    "not work on any platform, and (b) it would not be possible to close a "
    "menu by clicking away from it and into e.g. the omnibox.";

// Determines whether the current system is Linux + Wayland and the current test
// should be skipped for reasons described in the error message above.
bool SkipIfLinuxWayland() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return views::test::InteractionTestUtilSimulatorViews::IsWayland();
#else
  return false;
#endif
}

}  // namespace

#if BUILDFLAG(IS_LINUX)
// For some reason, windows in Linux builds tend to either move around or
// misreport their positions; it's not clear why this happens, but it can
// (rarely) cause the test to flake. See e.g. crbug.com/349545780.
#define MAYBE_AnnotateMenu DISABLED_AnnotateMenu
#else
#define MAYBE_AnnotateMenu AnnotateMenu
#endif
// This is a combined test for both help bubbles anchored to menus and menu
// annotation.
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest, MAYBE_AnnotateMenu) {
  // See message for why this is necessary.
  if (SkipIfLinuxWayland()) {
    GTEST_SKIP_(kLinuxWaylandErrorMessage);
  }

  UNCALLED_MOCK_CALLBACK(base::OnceClosure, default_button_clicked);
  constexpr char16_t kButton1Text[] = u"button 1";

  user_education::HelpBubbleParams params = GetBubbleParams();

  params.arrow = user_education::HelpBubbleArrow::kRightCenter;

  user_education::HelpBubbleButtonParams button1;
  button1.text = kButton1Text;
  button1.is_default = true;
  button1.callback = default_button_clicked.Get();
  params.buttons.emplace_back(std::move(button1));

  EXPECT_CALL(default_button_clicked, Run).Times(1);

  RunTestSequence(
      // Show the application menu and attach a bubble to a menu item.
      PressButton(kToolbarAppMenuButtonElementId),

      // There may be some shuffling and setting up on some platforms (looking
      // at you, Lacros) so make sure the menu is fully loaded before trying to
      // show the help bubble.
      WaitForShow(AppMenuModel::kDownloadsMenuItem),

      // Show the help bubble attached to the menu.
      ShowHelpBubble(AppMenuModel::kDownloadsMenuItem, std::move(params)),

      // Hover the default button and verify that the inkdrop is highlighted.
      MoveMouseTo(HelpBubbleView::kDefaultButtonIdForTesting),

      // TODO(dfried): figure out if we can determine if an inkdrop is in a
      // hovered state; currently that information can't be accessed.

      // Click the default button and verify that the help bubble closes but the
      // menu does not.
      ClickMouse(), WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(AppMenuModel::kDownloadsMenuItem));
}

// Verifies that we can safely show and then close two help bubbles attached to
// the same menu. This may happen transiently during tutorials.
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest, TwoMenuHelpBubbles) {
  // See message for why this is necessary.
  if (SkipIfLinuxWayland()) {
    GTEST_SKIP_(kLinuxWaylandErrorMessage);
  }

  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button_clicked);
  constexpr char16_t kButtonText[] = u"button";

  // First bubble has no buttons.
  auto params1 = GetBubbleParams();
  params1.arrow = user_education::HelpBubbleArrow::kRightCenter;

  // Second bubble has a default button.
  auto params2 = GetBubbleParams();
  params2.arrow = user_education::HelpBubbleArrow::kRightCenter;

  user_education::HelpBubbleButtonParams button;
  button.text = kButtonText;
  button.is_default = true;
  button.callback = button_clicked.Get();
  params2.buttons.emplace_back(std::move(button));

  EXPECT_CALL(button_clicked, Run).Times(1);

  RunTestSequence(
      // Show the application menu and attach a bubble to two different menu
      // items.
      PressButton(kToolbarAppMenuButtonElementId),

      // There may be some shuffling and setting up on some platforms (looking
      // at you, Lacros) so make sure the menu is fully loaded before trying to
      // show the help bubble.
      WaitForShow(AppMenuModel::kDownloadsMenuItem),

      ShowHelpBubble(AppMenuModel::kDownloadsMenuItem, std::move(params1)),
      ShowHelpBubble(AppMenuModel::kMoreToolsMenuItem, std::move(params2)),

      // Use the mouse to click the default button on the second bubble and wait
      // for the bubble to disappear.
      //
      // The default button should be targetable because it is at the bottom of
      // the lower of the two help bubbles.
      MoveMouseTo(HelpBubbleView::kDefaultButtonIdForTesting), ClickMouse(),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting)
          .SetTransitionOnlyOnEvent(true),

      // Close the remaining help bubble.
      CloseHelpBubble());
}

// Verifies that a help bubble can attach to a bubble which cannot activate or
// receive events, and that events are still routed correctly to the help
// bubble.
#if BUILDFLAG(IS_LINUX)
// For some reason, windows in Linux builds tend to either move around or
// misreport their positions; it's not clear why this happens, but it can
// (rarely) cause the test to flake. See e.g. crbug.com/349545780.
#define MAYBE_AnchorToTransparentBubble DISABLED_AnchorToTransparentBubble
#else
#define MAYBE_AnchorToTransparentBubble AnchorToTransparentBubble
#endif
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest,
                       MAYBE_AnchorToTransparentBubble) {
  // See message for why this is necessary.
  if (SkipIfLinuxWayland()) {
    GTEST_SKIP_(kLinuxWaylandErrorMessage);
  }

  UNCALLED_MOCK_CALLBACK(base::OnceClosure, default_button_clicked);
  constexpr char16_t kButton1Text[] = u"button 1";

  user_education::HelpBubbleParams params = GetBubbleParams();

  params.arrow = user_education::HelpBubbleArrow::kLeftTop;

  user_education::HelpBubbleButtonParams button1;
  button1.text = kButton1Text;
  button1.is_default = true;
  button1.callback = default_button_clicked.Get();
  params.buttons.emplace_back(std::move(button1));

  EXPECT_CALL(default_button_clicked, Run).Times(1);

  raw_ptr<views::Widget> widget = nullptr;

  RunTestSequence(
      // Make sure the window isn't at the origin.
      WithView(kBrowserViewElementId,
               [](views::View* view) {
                 view->GetWidget()->SetBounds(
                     gfx::Rect({50, 50}, view->GetWidget()->GetSize()));
               }),

      // Create the test bubble that cannot be activated.
      WithView(ContentsWebView::kContentsWebViewElementId,
               [&widget](views::View* view) {
                 widget = views::BubbleDialogDelegateView::CreateBubble(
                     std::make_unique<TestBubbleView>(view));
                 widget->ShowInactive();
               }),
      WaitForShow(kTestBubbleElementId),

      // Show a help bubble attached to the bubble.
      ShowHelpBubble(kTestBubbleElementId, std::move(params)),

      // Click the default button and verify that the help bubble closes.
      MoveMouseTo(HelpBubbleView::kDefaultButtonIdForTesting), ClickMouse(),

      // At this point the help bubble should close.
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),

      // Close the extra bubble.
      Do([&widget]() {
        widget->Close();
        widget = nullptr;
      }),
      WaitForHide(kTestBubbleElementId));
}
