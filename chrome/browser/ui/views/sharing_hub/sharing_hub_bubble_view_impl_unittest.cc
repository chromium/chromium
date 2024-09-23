// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "base/containers/adapters.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/sharing_hub/fake_sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"

using ::testing::Truly;

namespace {

void EnumerateDescendants(views::View* root,
                          std::vector<views::View*>& result) {
  result.push_back(root);
  for (views::View* child : root->children()) {
    EnumerateDescendants(child, result);
  }
}

std::vector<views::View*> DescendantsMatchingPredicate(
    views::View* root,
    base::RepeatingCallback<bool(views::View*)> predicate) {
  std::vector<views::View*> descendants;
  std::vector<views::View*> result;

  EnumerateDescendants(root, descendants);
  base::ranges::copy_if(descendants, std::back_inserter(result),
                        [=](views::View* view) { return predicate.Run(view); });
  return result;
}

bool ViewHasClassName(const std::string& class_name, views::View* view) {
  return view->GetClassName() == class_name;
}

std::string AccessibleNameForView(views::View* view) {
  ui::AXNodeData data;
  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  return data.GetStringAttribute(ax::mojom::StringAttribute::kName);
}

void Click(views::Button* button) {
  button->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(1, 1),
                     gfx::Point(0, 0), base::TimeTicks::Now(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  button->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(1, 1),
                     gfx::Point(0, 0), base::TimeTicks::Now(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
}

void SendKeyPress(views::Widget* widget, ui::KeyboardCode key_code) {
  ui::KeyEvent press(ui::EventType::kKeyPressed, key_code, 0,
                     base::TimeTicks::Now());
  widget->OnKeyEvent(&press);
  ui::KeyEvent release(ui::EventType::kKeyReleased, key_code, 0,
                       base::TimeTicks::Now());
  widget->OnKeyEvent(&release);
}

views::View* FocusedViewOf(views::Widget* widget) {
  return widget->GetFocusManager()->GetFocusedView();
}

const gfx::VectorIcon kEmptyIcon;

const std::vector<sharing_hub::SharingHubAction> kFirstPartyActions = {
    {0, u"Feed to Dino", &kEmptyIcon, "feed-to-dino", 0},
    {1, u"Reverse Star", &kEmptyIcon, "reverse-star", 0},
    {2, u"Pastelify", &kEmptyIcon, "pastelify", 0},
};

}  // namespace

class SharingHubBubbleTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  }

  void TearDown() override {
    bubble_widget_->CloseNow();
    anchor_widget_->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

  void ShowBubble() {
    auto bubble = std::make_unique<sharing_hub::SharingHubBubbleViewImpl>(
        anchor_widget_->GetRootView(),
        share::ShareAttempt(nullptr, u"Hello!",
                            GURL("https://www.chromium.org"), ui::ImageModel()),
        &controller_);
    bubble_ = bubble.get();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
    bubble_->ShowForReason(sharing_hub::SharingHubBubbleViewImpl::USER_GESTURE);
    bubble_widget_ = bubble_->GetWidget();
  }

  sharing_hub::SharingHubBubbleViewImpl* bubble() { return bubble_; }
  views::Widget* bubble_widget() { return bubble_widget_.get(); }

  sharing_hub::FakeSharingHubBubbleController* controller() {
    return &controller_;
  }

  std::vector<sharing_hub::SharingHubBubbleActionButton*> GetActionButtons() {
    return base::ToVector(
        DescendantsMatchingPredicate(
            bubble(), base::BindRepeating(&ViewHasClassName,
                                          "SharingHubBubbleActionButton")),
        [](views::View* view) {
          return static_cast<sharing_hub::SharingHubBubbleActionButton*>(view);
        });
  }

 private:
  raw_ptr<sharing_hub::SharingHubBubbleViewImpl, DanglingUntriaged> bubble_;
  testing::NiceMock<sharing_hub::FakeSharingHubBubbleController> controller_{
      kFirstPartyActions};

  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<views::Widget, DanglingUntriaged> bubble_widget_;
};

TEST_F(SharingHubBubbleTest, AllFirstPartyActionsAppearInOrder) {
  ShowBubble();

  auto actions = GetActionButtons();
  ASSERT_GE(actions.size(), 3u);
  EXPECT_EQ(AccessibleNameForView(actions[0]), "Feed to Dino");
  EXPECT_EQ(AccessibleNameForView(actions[1]), "Reverse Star");
  EXPECT_EQ(AccessibleNameForView(actions[2]), "Pastelify");
}

TEST_F(SharingHubBubbleTest, ClickingActionsCallsController) {
  ShowBubble();

  constexpr auto HasRightCommandId =
      [](const sharing_hub::SharingHubAction& action) {
        return action.command_id == 2;
      };

  auto actions = GetActionButtons();
  ASSERT_GE(actions.size(), 3u);
  EXPECT_CALL(*controller(), OnActionSelected(Truly(HasRightCommandId)));
  Click(actions[2]);
}

TEST_F(SharingHubBubbleTest, ArrowKeysTraverseItemsForward) {
  ShowBubble();
  EXPECT_EQ(nullptr, FocusedViewOf(bubble_widget()));

  auto actions = GetActionButtons();
  // Don't allow this test (and the below test) to pass vacuously if there
  // are zero actions to traverse.
  ASSERT_GT(actions.size(), 0u);
  for (auto* button : GetActionButtons()) {
    SendKeyPress(bubble_widget(), ui::VKEY_DOWN);
    EXPECT_EQ(button, FocusedViewOf(bubble_widget()));
  }
}

TEST_F(SharingHubBubbleTest, ArrowKeysTraverseItemsBackward) {
  ShowBubble();
  EXPECT_EQ(nullptr, FocusedViewOf(bubble_widget()));

  auto actions = GetActionButtons();
  ASSERT_GT(actions.size(), 0u);
  for (auto* button : base::Reversed(actions)) {
    SendKeyPress(bubble_widget(), ui::VKEY_UP);
    EXPECT_EQ(button, FocusedViewOf(bubble_widget()));
  }
}
