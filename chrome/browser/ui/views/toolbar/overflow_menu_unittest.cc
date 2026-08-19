// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/overflow_menu.h"

#include <map>
#include <memory>
#include <set>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

namespace {

using ElementIdInfo = OverflowMenu::ElementIdInfo;
using ResponsiveElementInfo = OverflowMenu::ResponsiveElementInfo;
using ActionId = actions::ActionId;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton3);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyActivateView);

// Similar to a OverflowMenu::OverflowableElement, but uses an
// ui::ElementIdentifier instead of an ElementIdInfo, for better
// indexing.
using OverflowableElementId = std::variant<ui::ElementIdentifier, ActionId>;

// Gets the OverflowableElementId for an OverflowMenu::OverflowableElement.
OverflowableElementId GetOverflowableElementId(
    const OverflowMenu::OverflowableElement& element) {
  return std::visit(
      absl::Overload{[](ActionId id) -> OverflowableElementId { return id; },
                     [](const ElementIdInfo& info) -> OverflowableElementId {
                       return info.overflow_identifier;
                     }},
      element);
}

class TestPinnedActionsInfo : public OverflowMenu::PinnedActionsInfo {
 public:
  MOCK_METHOD(void,
              DummyAction,
              (actions::ActionItem*, actions::ActionInvocationContext));

  // If `model` is non-null, then the object's PinnedActionIds() will wrap the
  // model's PinnedActionIds() method.
  explicit TestPinnedActionsInfo(
      const PinnedToolbarActionsModel* model = nullptr)
      : model_(model) {}

  ~TestPinnedActionsInfo() override = default;

  actions::ActionItem* GetActionItemFor(ActionId id) override {
    for (const auto& action_item : action_items_) {
      if (action_item->GetActionId() == id) {
        return action_item.get();
      }
    }
    action_items_.push_back(
        actions::ActionItem::ActionItemBuilder(
            base::BindRepeating(&TestPinnedActionsInfo::DummyAction,
                                base::Unretained(this)))
            .SetActionId(id)
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? vector_icons::kPetsIcon
                    : vector_icons::kDogfoodOldIcon))
            .SetProperty(kActionItemUnderlineIndicatorKey, true)
            .SetText(base::StrCat({u"DummyAction", base::NumberToString16(id)}))
            .Build());
    return action_items_.back().get();
  }

  const std::vector<ActionId>& PinnedActionIds() const override {
    if (model_) {
      return model_->PinnedActionIds();
    }
    return action_ids_;
  }

 private:
  const std::vector<ActionId> action_ids_ = {0, 1, 2};
  const raw_ptr<const PinnedToolbarActionsModel> model_ = nullptr;
  std::vector<std::unique_ptr<actions::ActionItem>> action_items_;
};

class TestOverflowMenuDelegate : public OverflowMenu::Delegate {
 public:
  explicit TestOverflowMenuDelegate(TestPinnedActionsInfo* pinned_actions_info)
      : pinned_actions_info_(pinned_actions_info) {}
  ~TestOverflowMenuDelegate() override = default;

  void SetOverflowed(const OverflowableElementId& id, bool overflowed) {
    if (overflowed) {
      overflowed_elements_.insert(id);
    } else {
      overflowed_elements_.erase(id);
    }
  }

  void SetOverflowed(const ResponsiveElementInfo& element, bool overflowed) {
    std::visit(
        absl::Overload{[&](ActionId id) { SetOverflowed(id, overflowed); },
                       [&](ElementIdInfo info) {
                         SetOverflowed(info.overflow_identifier, overflowed);
                       }},
        element.overflow_id);
  }

  void SetEnabled(const OverflowableElementId& id, bool enabled) {
    if (!enabled) {
      disabled_elements_.insert(id);
    } else {
      disabled_elements_.erase(id);
    }
  }

  void SetEnabled(const ResponsiveElementInfo& element, bool enabled) {
    std::visit(absl::Overload{[&](ActionId id) { SetEnabled(id, enabled); },
                              [&](ElementIdInfo info) {
                                SetEnabled(info.overflow_identifier, enabled);
                              }},
               element.overflow_id);
  }

  void ExecuteCommand(
      const OverflowMenu::OverflowableElement& element) override {
    executed_elements_.push_back(element);
    if (const auto* action_id = std::get_if<ActionId>(&element)) {
      if (pinned_actions_info_) {
        pinned_actions_info_->GetActionItemFor(*action_id)
            ->InvokeAction(actions::ActionInvocationContext::Builder().Build());
      }
    }
  }

  bool IsCurrentlyOverflowed(
      const OverflowMenu::OverflowableElement& element) const override {
    return overflowed_elements_.contains(GetOverflowableElementId(element));
  }

  bool IsEnabled(
      const OverflowMenu::OverflowableElement& element) const override {
    return !disabled_elements_.contains(GetOverflowableElementId(element));
  }

  const std::vector<OverflowMenu::OverflowableElement>& executed_elements()
      const {
    return executed_elements_;
  }

 private:
  raw_ptr<TestPinnedActionsInfo> pinned_actions_info_;
  std::set<OverflowableElementId> overflowed_elements_;
  std::set<OverflowableElementId> disabled_elements_;
  std::vector<OverflowMenu::OverflowableElement> executed_elements_;
};

}  // namespace

class OverflowMenuTest : public ChromeViewsTestBase {
 public:
  OverflowMenuTest() = default;
  ~OverflowMenuTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    pinned_actions_model_ =
        std::make_unique<PinnedToolbarActionsModel>(profile_.get());
    pinned_actions_info_ = std::make_unique<TestPinnedActionsInfo>();
    delegate_ =
        std::make_unique<TestOverflowMenuDelegate>(pinned_actions_info_.get());
  }

  void TearDown() override {
    delegate_.reset();
    pinned_actions_info_.reset();
    pinned_actions_model_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestOverflowMenuDelegate* delegate() { return delegate_.get(); }
  TestPinnedActionsInfo* pinned_actions_info() {
    return pinned_actions_info_.get();
  }
  PinnedToolbarActionsModel* pinned_actions_model() {
    return pinned_actions_model_.get();
  }

  std::unique_ptr<OverflowMenu> CreateOverflowMenu(
      const std::vector<ResponsiveElementInfo>& responsive_elements) {
    auto overflow_menu = std::make_unique<OverflowMenu>(
        responsive_elements, delegate_.get(), pinned_actions_info_.get(),
        pinned_actions_model_.get());
    overflow_menu->set_menu_text_callback_for_testing(base::BindRepeating(
        [](const ResponsiveElementInfo& element_info) -> std::u16string {
          if (const auto* id_info =
                  std::get_if<ElementIdInfo>(&element_info.overflow_id)) {
            static const base::flat_map<ui::ElementIdentifier, std::u16string>
                kMap = {{kDummyButton1, u"DummyButton1"},
                        {kDummyButton2, u"DummyButton2"},
                        {kDummyButton3, u"DummyButton3"}};
            auto it = kMap.find(id_info->overflow_identifier);
            if (it != kMap.end()) {
              return it->second;
            }
          }
          return u"";
        }));
    return overflow_menu;
  }

  bool IsOverflowed(const ResponsiveElementInfo& element) {
    return delegate()->IsCurrentlyOverflowed(element.overflow_id);
  }

  static ResponsiveElementInfo ResponsiveElementInfoForDummyElementId(
      ui::ElementIdentifier id,
      bool is_section_end = false) {
    return ResponsiveElementInfo(
        ElementIdInfo{
            id, 0,
            &(features::IsRoundedIconsEnabled() ? vector_icons::kErrorFilledIcon
                                                : vector_icons::kErrorOldIcon),
            kDummyActivateView},
        is_section_end);
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PinnedToolbarActionsModel> pinned_actions_model_;
  std::unique_ptr<TestPinnedActionsInfo> pinned_actions_info_;
  std::unique_ptr<TestOverflowMenuDelegate> delegate_;
};

TEST_F(OverflowMenuTest, OverflowedButtonsMatchMenu) {
  auto element0 = ResponsiveElementInfoForDummyElementId(kDummyButton1);
  auto element1 = ResponsiveElementInfoForDummyElementId(kDummyButton2);
  auto element2 = ResponsiveElementInfoForDummyElementId(kDummyButton3);

  delegate()->SetOverflowed(element0, false);
  delegate()->SetOverflowed(element1, true);
  delegate()->SetOverflowed(element2, true);

  auto overflow_menu = CreateOverflowMenu({element0, element1, element2});
  auto widget = CreateTestWidget(
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  views::View* anchor_view =
      widget->SetContentsView(std::make_unique<views::View>());

  overflow_menu->ShowMenu(widget.get(), nullptr,
                          anchor_view->GetBoundsInScreen());
  const ui::SimpleMenuModel* menu = overflow_menu->menu_model_for_testing();

  // Overflowed buttons should match overflow menu.
  EXPECT_TRUE(menu);
  const auto& responsive_elements = overflow_menu->responsive_elements();
  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      EXPECT_EQ(overflow_menu->GetMenuText(responsive_elements[i]),
                menu->GetLabelAt(menu->GetIndexOfCommandId(i).value()));
    }
  }
}

TEST_F(OverflowMenuTest, RunningMenuAddsStatusIndicator) {
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));
  ResponsiveElementInfo action2(static_cast<ActionId>(2));

  delegate()->SetOverflowed(action0, false);
  delegate()->SetOverflowed(action1, true);
  delegate()->SetOverflowed(action2, true);

  auto overflow_menu = CreateOverflowMenu({action0, action1});
  auto widget = CreateTestWidget(
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  views::View* anchor_view =
      widget->SetContentsView(std::make_unique<views::View>());

  overflow_menu->ShowMenu(widget.get(), nullptr,
                          anchor_view->GetBoundsInScreen());
  EXPECT_TRUE(overflow_menu->IsMenuRunning());

  // Overflowed buttons should match overflow menu.
  views::SubmenuView* sub_menu = overflow_menu->root_menu_item()->GetSubmenu();

  for (auto* menu_item : sub_menu->GetMenuItems()) {
    PinnedToolbarButtonStatusIndicator* status_indicator = nullptr;

    for (auto& child : menu_item->icon_view()->children()) {
      if (views::AsViewClass<PinnedToolbarButtonStatusIndicator>(child)) {
        status_indicator =
            views::AsViewClass<PinnedToolbarButtonStatusIndicator>(child);
      }

      EXPECT_TRUE(status_indicator);
    }
  }
}

TEST_F(OverflowMenuTest, MenuSeparator) {
  auto element0 = ResponsiveElementInfoForDummyElementId(kDummyButton1);
  auto element1 = ResponsiveElementInfoForDummyElementId(
      kDummyButton2, /*is_section_end=*/true);
  auto element2 = ResponsiveElementInfoForDummyElementId(kDummyButton3);

  // All 3 buttons overflowed.
  delegate()->SetOverflowed(element0, true);
  delegate()->SetOverflowed(element1, true);
  delegate()->SetOverflowed(element2, true);

  auto overflow_menu = CreateOverflowMenu({element0, element1, element2});
  auto menu = overflow_menu->CreateMenuModel();
  EXPECT_TRUE(menu);

  // There is no separator between button1 and 2 because button1 is not a menu
  // section end.
  // There is a separator between button2 and button3 because
  // 1) button2 is a section end;
  // 2) the section button2 is in is valid;
  // 3) the section button3 is in is valid.
  // There is no separator after button3 because there is no valid next section.
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(4));
  EXPECT_EQ(menu->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(0), u"DummyButton1");
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(1), u"DummyButton2");
  EXPECT_EQ(menu->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetTypeAt(3), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(3), u"DummyButton3");
}

TEST_F(OverflowMenuTest, InvalidFirstSectionAddsNoLeadingSeparator) {
  auto element0 = ResponsiveElementInfoForDummyElementId(
      kDummyButton1, /*is_section_end=*/true);
  auto element1 = ResponsiveElementInfoForDummyElementId(
      kDummyButton2, /*is_section_end=*/true);
  auto element2 = ResponsiveElementInfoForDummyElementId(
      kDummyButton3, /*is_section_end=*/true);

  delegate()->SetOverflowed(element0, false);
  delegate()->SetOverflowed(element1, true);
  delegate()->SetOverflowed(element2, true);

  auto overflow_menu = CreateOverflowMenu({element0, element1, element2});
  auto menu = overflow_menu->CreateMenuModel();
  EXPECT_TRUE(menu);

  // The first section (contains Button1) is invalid. It should not add a
  // separator before Button2.
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(3));
  EXPECT_EQ(menu->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(0), u"DummyButton2");
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(2), u"DummyButton3");
}

TEST_F(OverflowMenuTest, InvalidSectionInMiddleAddsNoExtraSeparator) {
  auto element0 = ResponsiveElementInfoForDummyElementId(
      kDummyButton1, /*is_section_end=*/true);
  auto element1 = ResponsiveElementInfoForDummyElementId(
      kDummyButton2, /*is_section_end=*/true);
  auto element2 = ResponsiveElementInfoForDummyElementId(
      kDummyButton3, /*is_section_end=*/true);

  delegate()->SetOverflowed(element0, true);
  delegate()->SetOverflowed(element1, false);
  delegate()->SetOverflowed(element2, true);

  auto overflow_menu = CreateOverflowMenu({element0, element1, element2});
  auto menu = overflow_menu->CreateMenuModel();
  EXPECT_TRUE(menu);

  // The second section (contains Button2) is invalid. It should not add a
  // redundant separator.
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(3));
  EXPECT_EQ(menu->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(0), u"DummyButton1");
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(2), u"DummyButton3");
}

TEST_F(OverflowMenuTest, InvalidLastSectionAddsNoTrailingSeparator) {
  auto element0 = ResponsiveElementInfoForDummyElementId(
      kDummyButton1, /*is_section_end=*/true);
  auto element1 = ResponsiveElementInfoForDummyElementId(
      kDummyButton2, /*is_section_end=*/true);
  auto element2 = ResponsiveElementInfoForDummyElementId(
      kDummyButton3, /*is_section_end=*/true);

  delegate()->SetOverflowed(element0, true);
  delegate()->SetOverflowed(element1, true);
  delegate()->SetOverflowed(element2, false);

  auto overflow_menu = CreateOverflowMenu({element0, element1, element2});
  auto menu = overflow_menu->CreateMenuModel();
  EXPECT_TRUE(menu);

  // The third section (contains Button3) is invalid. It should not add a
  // redundant trailing separator.
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(3));
  EXPECT_EQ(menu->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(0), u"DummyButton1");
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(2), u"DummyButton2");
}

TEST_F(OverflowMenuTest, MenuItemUsability) {
  auto element0 = ResponsiveElementInfoForDummyElementId(kDummyButton1);
  auto element1 = ResponsiveElementInfoForDummyElementId(kDummyButton2);
  auto element2 = ResponsiveElementInfoForDummyElementId(kDummyButton3);

  delegate()->SetOverflowed(element0, false);
  delegate()->SetOverflowed(element1, true);
  delegate()->SetOverflowed(element2, true);

  delegate()->SetEnabled(element2, false);

  auto overflow_menu = CreateOverflowMenu({element0, element1, element2});
  auto widget = CreateTestWidget(
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  views::View* anchor_view =
      widget->SetContentsView(std::make_unique<views::View>());

  overflow_menu->ShowMenu(widget.get(), nullptr,
                          anchor_view->GetBoundsInScreen());
  const ui::SimpleMenuModel* menu = overflow_menu->menu_model_for_testing();
  EXPECT_TRUE(menu);

  const auto& responsive_elements = overflow_menu->responsive_elements();
  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      EXPECT_EQ(delegate()->IsEnabled(responsive_elements[i].overflow_id),
                menu->IsEnabledAt(menu->GetIndexOfCommandId(i).value()));
    }
  }
}

TEST_F(OverflowMenuTest, ResponsiveActionsAreOrdered) {
  auto element0 = ResponsiveElementInfoForDummyElementId(kDummyButton1);
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));
  ResponsiveElementInfo action2(static_cast<ActionId>(2));

  auto overflow_menu = CreateOverflowMenu(
      {action2, action1, action0, element0, action2, action0});
  std::vector<ResponsiveElementInfo> elements =
      overflow_menu->GetResponsiveElementsWithOrderedActions();
  EXPECT_EQ(int(elements.size()), 6);

  // Both sections of actions are reordered
  EXPECT_EQ(std::get<ActionId>(elements[0].overflow_id),
            std::get<ActionId>(action0.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[1].overflow_id),
            std::get<ActionId>(action1.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[2].overflow_id),
            std::get<ActionId>(action2.overflow_id));
  EXPECT_EQ(
      std::get<ElementIdInfo>(elements[3].overflow_id).overflow_identifier,
      std::get<ElementIdInfo>(element0.overflow_id).overflow_identifier);
  EXPECT_EQ(std::get<ActionId>(elements[4].overflow_id),
            std::get<ActionId>(action0.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[5].overflow_id),
            std::get<ActionId>(action2.overflow_id));
}

TEST_F(OverflowMenuTest, ResponsiveActionsRemainOrdered) {
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));

  PinnedToolbarActionsModel* model = pinned_actions_model();
  model->UpdatePinnedState(std::get<ActionId>(action0.overflow_id), true);
  model->UpdatePinnedState(std::get<ActionId>(action1.overflow_id), true);

  auto model_info =
      std::make_unique<TestPinnedActionsInfo>(pinned_actions_model());

  // Create the OverflowMenu with the ActionIds in the reversed order
  // (action1, action0) with respect to delegate.PinnedActionIds().
  // They should be sorted in responsive_elements right after the OverflowMenu
  // is instantiated.
  auto overflow_menu = std::make_unique<OverflowMenu>(
      std::vector<ResponsiveElementInfo>{action1, action0}, delegate(),
      model_info.get(), pinned_actions_model());

  std::vector<ResponsiveElementInfo> elements =
      overflow_menu->responsive_elements();
  EXPECT_EQ(int(elements.size()), 2);
  EXPECT_EQ(std::get<ActionId>(elements[0].overflow_id),
            std::get<ActionId>(action0.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[1].overflow_id),
            std::get<ActionId>(action1.overflow_id));

  // Move action1 to the first index. responsive_elements should be reordered.
  model->MovePinnedAction(std::get<ActionId>(action1.overflow_id), 0);
  elements = overflow_menu->responsive_elements();
  EXPECT_EQ(int(elements.size()), 2);
  EXPECT_EQ(std::get<ActionId>(elements[0].overflow_id),
            std::get<ActionId>(action1.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[1].overflow_id),
            std::get<ActionId>(action0.overflow_id));
}

TEST_F(OverflowMenuTest, ResponsiveActionsAreNotOrdered) {
  auto element0 = ResponsiveElementInfoForDummyElementId(kDummyButton1);
  auto element1 = ResponsiveElementInfoForDummyElementId(kDummyButton2);
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));
  ResponsiveElementInfo action2(static_cast<ActionId>(2));

  auto overflow_menu = CreateOverflowMenu(std::vector<ResponsiveElementInfo>(
      {element1, element0, action2, element0, action0, element0, action1}));

  std::vector<ResponsiveElementInfo> elements =
      overflow_menu->GetResponsiveElementsWithOrderedActions();
  EXPECT_EQ(int(elements.size()), 7);

  // Only sections of actions are reordered, so we
  // expect the order not to change
  EXPECT_EQ(
      std::get<ElementIdInfo>(elements[0].overflow_id).overflow_identifier,
      std::get<ElementIdInfo>(element1.overflow_id).overflow_identifier);
  EXPECT_EQ(
      std::get<ElementIdInfo>(elements[1].overflow_id).overflow_identifier,
      std::get<ElementIdInfo>(element0.overflow_id).overflow_identifier);
  EXPECT_EQ(std::get<ActionId>(elements[2].overflow_id),
            std::get<ActionId>(action2.overflow_id));
  EXPECT_EQ(
      std::get<ElementIdInfo>(elements[3].overflow_id).overflow_identifier,
      std::get<ElementIdInfo>(element0.overflow_id).overflow_identifier);
  EXPECT_EQ(std::get<ActionId>(elements[4].overflow_id),
            std::get<ActionId>(action0.overflow_id));
  EXPECT_EQ(
      std::get<ElementIdInfo>(elements[5].overflow_id).overflow_identifier,
      std::get<ElementIdInfo>(element0.overflow_id).overflow_identifier);
  EXPECT_EQ(std::get<ActionId>(elements[6].overflow_id),
            std::get<ActionId>(action1.overflow_id));
}

TEST_F(OverflowMenuTest, PinnedAndUnpinnedOverflowedActionsDivided) {
  // Add 4 overflowed actions, 2 pinned and 2 unpinned.
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));
  ResponsiveElementInfo action2(static_cast<ActionId>(2));
  ResponsiveElementInfo action3(static_cast<ActionId>(3));

  PinnedToolbarActionsModel* model = pinned_actions_model();
  model->UpdatePinnedState(std::get<ActionId>(action0.overflow_id), true);
  model->UpdatePinnedState(std::get<ActionId>(action1.overflow_id), true);

  auto model_info =
      std::make_unique<TestPinnedActionsInfo>(pinned_actions_model());

  auto overflow_menu = std::make_unique<OverflowMenu>(
      std::vector<ResponsiveElementInfo>{action2, action0, action1, action3},
      delegate(), model_info.get(), model);

  std::vector<ResponsiveElementInfo> elements =
      overflow_menu->GetResponsiveElementsWithOrderedActions();

  // Expect elements are sorted pinned then unpinned.
  ASSERT_EQ(int(elements.size()), 4);
  EXPECT_EQ(std::get<ActionId>(elements[0].overflow_id),
            std::get<ActionId>(action0.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[1].overflow_id),
            std::get<ActionId>(action1.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[2].overflow_id),
            std::get<ActionId>(action2.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[3].overflow_id),
            std::get<ActionId>(action3.overflow_id));

  // Check section ends for pinned and unpinned actions.
  EXPECT_FALSE(elements[0].is_section_end);
  EXPECT_TRUE(elements[1].is_section_end);
  EXPECT_FALSE(elements[2].is_section_end);
  EXPECT_TRUE(elements[3].is_section_end);
}

TEST_F(OverflowMenuTest, PinnedOverflowedActionsDividedWithNoUnpinnedActions) {
  // Add 2 overflowed actions, both pinned.
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));

  // Add a trailing non-action element.
  auto trailing_element = ResponsiveElementInfoForDummyElementId(kDummyButton2);

  PinnedToolbarActionsModel* model = pinned_actions_model();
  model->UpdatePinnedState(std::get<ActionId>(action0.overflow_id), true);
  model->UpdatePinnedState(std::get<ActionId>(action1.overflow_id), true);

  auto model_info =
      std::make_unique<TestPinnedActionsInfo>(pinned_actions_model());

  auto overflow_menu = std::make_unique<OverflowMenu>(
      std::vector<ResponsiveElementInfo>{action1, action0, trailing_element},
      delegate(), model_info.get(), model);

  std::vector<ResponsiveElementInfo> elements =
      overflow_menu->GetResponsiveElementsWithOrderedActions();

  ASSERT_EQ(int(elements.size()), 3);
  EXPECT_EQ(std::get<ActionId>(elements[0].overflow_id),
            std::get<ActionId>(action0.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[1].overflow_id),
            std::get<ActionId>(action1.overflow_id));
  EXPECT_EQ(
      std::get<ElementIdInfo>(elements[2].overflow_id).overflow_identifier,
      kDummyButton2);

  // Check section ends for only pinned actions.
  EXPECT_FALSE(elements[0].is_section_end);
  EXPECT_TRUE(elements[1].is_section_end);
  EXPECT_FALSE(elements[2].is_section_end);
}

TEST_F(OverflowMenuTest, UnpinnedOverflowedActionsDividedWithNoPinnedActions) {
  // Add 2 overflowed actions, both unpinned.
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));

  // Add a trailing non-action element.
  ResponsiveElementInfo trailing_element(
      ResponsiveElementInfoForDummyElementId(kDummyButton2));

  PinnedToolbarActionsModel* model = pinned_actions_model();
  auto model_info =
      std::make_unique<TestPinnedActionsInfo>(pinned_actions_model());

  auto overflow_menu = std::make_unique<OverflowMenu>(
      std::vector<ResponsiveElementInfo>({action1, action0, trailing_element}),
      delegate(), model_info.get(), model);

  std::vector<ResponsiveElementInfo> elements =
      overflow_menu->GetResponsiveElementsWithOrderedActions();

  ASSERT_EQ(int(elements.size()), 3);
  EXPECT_EQ(std::get<ActionId>(elements[0].overflow_id),
            std::get<ActionId>(action1.overflow_id));
  EXPECT_EQ(std::get<ActionId>(elements[1].overflow_id),
            std::get<ActionId>(action0.overflow_id));
  EXPECT_EQ(
      std::get<ElementIdInfo>(elements[2].overflow_id).overflow_identifier,
      kDummyButton2);

  // Check section ends for only unpinned actions.
  EXPECT_FALSE(elements[0].is_section_end);
  EXPECT_TRUE(elements[1].is_section_end);
  EXPECT_FALSE(elements[2].is_section_end);
}

TEST_F(OverflowMenuTest, SupportActionIds) {
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));
  ResponsiveElementInfo action2(static_cast<ActionId>(2));

  delegate()->SetOverflowed(action0, false);
  delegate()->SetOverflowed(action1, true);
  delegate()->SetOverflowed(action2, true);

  auto overflow_menu = CreateOverflowMenu({action0, action1, action2});
  const auto menu = overflow_menu->CreateMenuModel();
  EXPECT_TRUE(menu);
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(2));

  // Overflowed actions should match overflow menu.
  const auto& responsive_elements = overflow_menu->responsive_elements();
  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      size_t index = menu->GetIndexOfCommandId(i).value();
      EXPECT_EQ(overflow_menu->GetMenuText(responsive_elements[i]),
                menu->GetLabelAt(index));
      EXPECT_CALL(*pinned_actions_info(), DummyAction);
      menu->ActivatedAt(index);
    }
  }
}

TEST_F(OverflowMenuTest, StatusIndicatorVisibilityUpdates) {
  ResponsiveElementInfo action0(static_cast<ActionId>(0));
  ResponsiveElementInfo action1(static_cast<ActionId>(1));
  ResponsiveElementInfo action2(static_cast<ActionId>(2));

  delegate()->SetOverflowed(action0, false);
  delegate()->SetOverflowed(action1, true);
  delegate()->SetOverflowed(action2, true);

  auto overflow_menu = CreateOverflowMenu({action0, action1, action2});
  auto widget = CreateTestWidget(
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  views::View* anchor_view =
      widget->SetContentsView(std::make_unique<views::View>());

  overflow_menu->ShowMenu(widget.get(), nullptr,
                          anchor_view->GetBoundsInScreen());
  EXPECT_TRUE(overflow_menu->IsMenuRunning());

  const ui::SimpleMenuModel* menu = overflow_menu->menu_model_for_testing();

  // Overflowed buttons should match overflow menu.
  EXPECT_TRUE(menu);
  views::SubmenuView* sub_menu = overflow_menu->root_menu_item()->GetSubmenu();

  for (auto* menu_item : sub_menu->GetMenuItems()) {
    PinnedToolbarButtonStatusIndicator* status_indicator =
        PinnedToolbarButtonStatusIndicator::GetStatusIndicator(
            menu_item->icon_view());
    EXPECT_TRUE(status_indicator);
    EXPECT_EQ(status_indicator->GetVisible(), true);
  }

  const auto& responsive_elements = overflow_menu->responsive_elements();
  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      ActionId element_action_id =
          std::get<ActionId>(responsive_elements[i].overflow_id);
      pinned_actions_info()
          ->GetActionItemFor(element_action_id)
          ->SetProperty(kActionItemUnderlineIndicatorKey, false);

      size_t index = menu->GetIndexOfCommandId(i).value();

      views::MenuItemView* menu_item = sub_menu->GetMenuItemAt(index);
      PinnedToolbarButtonStatusIndicator* status_indicator =
          PinnedToolbarButtonStatusIndicator::GetStatusIndicator(
              menu_item->icon_view());
      EXPECT_EQ(status_indicator->GetVisible(), false);
    }
  }

  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      ActionId element_action_id =
          std::get<ActionId>(responsive_elements[i].overflow_id);
      pinned_actions_info()
          ->GetActionItemFor(element_action_id)
          ->SetProperty(kActionItemUnderlineIndicatorKey, true);

      size_t index = menu->GetIndexOfCommandId(i).value();

      views::MenuItemView* menu_item = sub_menu->GetMenuItemAt(index);
      PinnedToolbarButtonStatusIndicator* status_indicator =
          PinnedToolbarButtonStatusIndicator::GetStatusIndicator(
              menu_item->icon_view());
      EXPECT_EQ(status_indicator->GetVisible(), true);
    }
  }
}
