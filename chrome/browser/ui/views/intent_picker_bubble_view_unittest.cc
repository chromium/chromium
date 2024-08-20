// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#endif

using AppInfo = apps::IntentPickerAppInfo;
using BubbleType = apps::IntentPickerBubbleType;
using content::WebContents;
using content::OpenURLParams;
using content::Referrer;

class IntentPickerBubbleViewTest : public TestWithBrowserView {
 public:
  IntentPickerBubbleViewTest() {
    feature_list_.InitWithFeatures(
        {}, apps::test::GetFeaturesToDisableLinkCapturingUX());
  }

  IntentPickerBubbleViewTest(const IntentPickerBubbleViewTest&) = delete;
  IntentPickerBubbleViewTest& operator=(const IntentPickerBubbleViewTest&) =
      delete;

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    if (bubble_)
      bubble_->GetWidget()->CloseNow();

    TestWithBrowserView::TearDown();
  }

 protected:
  void CreateBubbleView(bool use_icons,
                        bool show_stay_in_chrome,
                        BubbleType bubble_type,
                        const std::optional<url::Origin>& initiating_origin) {
    DCHECK(!app_info_.empty());
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    anchor_view_ =
        browser_view->toolbar()->AddChildView(std::make_unique<views::View>());

    if (use_icons)
      FillAppListWithDummyIcons();

    // We create |web_contents| since the Bubble UI has an Observer that
    // depends on this, otherwise it wouldn't work.
    GURL url("http://www.google.com");
    WebContents* web_contents = browser()->OpenURL(
        OpenURLParams(url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    CommitPendingLoad(&web_contents->GetController());

    auto* widget = IntentPickerBubbleView::ShowBubble(
        anchor_view_, /*highlighted_button=*/nullptr, bubble_type, web_contents,
        app_info_, show_stay_in_chrome,
        /*show_remember_selection=*/true, initiating_origin,
        base::BindOnce(&IntentPickerBubbleViewTest::OnBubbleClosed,
                       base::Unretained(this)));
    bubble_ = IntentPickerBubbleView::intent_picker_bubble();
    event_generator_.reset();  // Mac only allows one EventGenerator at a time.
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(widget));
  }

  // Add an app to the bubble opened by CreateBubbleView. Manually added apps
  // will appear before automatic placeholder apps.
  void AddApp(apps::PickerEntryType app_type,
              const std::string& launch_name,
              const std::string& title) {
    app_info_.emplace_back(app_type, ui::ImageModel(), launch_name, title);
  }

  void AddDefaultApps() {
    AddApp(apps::PickerEntryType::kArc, "package_1", "App 1");
    AddApp(apps::PickerEntryType::kArc, "package_2", "App 2");
  }

  void FillAppListWithDummyIcons() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    ui::ImageModel dummy_icon_model =
        ui::ImageModel::FromImage(rb.GetImageNamed(IDR_CLOSE));
    for (auto& app : app_info_)
      app.icon_model = dummy_icon_model;
  }

  views::Button* GetButtonAtIndex(size_t index) {
    auto children =
        bubble_->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
            ->children();
    CHECK_LT(index, children.size());
    return views::AsViewClass<views::Button>(children[index]);
  }

  views::LabelButton* GetLabelButtonAtIndex(size_t index) {
    CHECK(!apps::features::ShouldShowLinkCapturingUX());
    return views::AsViewClass<views::LabelButton>(GetButtonAtIndex(index));
  }

  void ClickApp(size_t index) {
    event_generator_->MoveMouseTo(
        GetButtonAtIndex(index)->GetBoundsInScreen().CenterPoint());
    event_generator_->ClickLeftButton();
  }

  views::InkDropState GetInkDropState(size_t index) {
    return views::InkDrop::Get(GetButtonAtIndex(index))
        ->GetInkDrop()
        ->GetTargetInkDropState();
  }

  size_t GetScrollViewSize() {
    return bubble_->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
        ->children()
        .size();
  }

  void OnBubbleClosed(const std::string& selected_app_launch_name,
                      apps::PickerEntryType entry_type,
                      apps::IntentPickerCloseReason close_reason,
                      bool should_persist) {
    last_selected_launch_name_ = selected_app_launch_name;
    last_close_reason_ = close_reason;
    last_selection_should_persist_ = should_persist;
  }

  base::test::ScopedFeatureList feature_list_;

  raw_ptr<IntentPickerBubbleView, DanglingUntriaged> bubble_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> anchor_view_;
  std::vector<AppInfo> app_info_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::string last_selected_launch_name_;
  apps::IntentPickerCloseReason last_close_reason_;
  bool last_selection_should_persist_;
};

// Verifies that we didn't set up an image for any LabelButton.
TEST_F(IntentPickerBubbleViewTest, NullIcons) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  size_t size = GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    gfx::ImageSkia image =
        GetLabelButtonAtIndex(i)->GetImage(views::Button::STATE_NORMAL);
    EXPECT_TRUE(image.isNull()) << i;
  }
}

// Verifies that all the icons contain a non-null icon.
TEST_F(IntentPickerBubbleViewTest, NonNullIcons) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  size_t size = GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    gfx::ImageSkia image =
        GetLabelButtonAtIndex(i)->GetImage(views::Button::STATE_NORMAL);
    EXPECT_FALSE(image.isNull()) << i;
  }
}

// Verifies that the bubble contains as many rows as |app_info_| with one
// exception, if the Chrome package is present on the input list it won't be
// shown to the user on the picker UI, so there could be a difference
// represented by |chrome_package_repetitions|.
TEST_F(IntentPickerBubbleViewTest, LabelsPtrVectorSize) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  size_t size = app_info_.size();

  EXPECT_EQ(size, GetScrollViewSize());
}

// Verifies that the first item is activated by default when creating a new
// bubble.
TEST_F(IntentPickerBubbleViewTest, VerifyStartingInkDrop) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  size_t size = GetScrollViewSize();
  EXPECT_EQ(GetInkDropState(0), views::InkDropState::ACTIVATED);
  for (size_t i = 1; i < size; ++i) {
    EXPECT_EQ(GetInkDropState(i), views::InkDropState::HIDDEN);
  }
}

// Press each button at a time and make sure it goes to ACTIVATED state,
// followed by HIDDEN state after selecting other button.
TEST_F(IntentPickerBubbleViewTest, InkDropStateTransition) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  size_t size = GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    ClickApp((i + 1) % size);
    EXPECT_EQ(GetInkDropState(i), views::InkDropState::HIDDEN);
    EXPECT_EQ(GetInkDropState((i + 1) % size), views::InkDropState::ACTIVATED);
  }
}

// Arbitrary press a button twice, check that the InkDropState remains the same.
TEST_F(IntentPickerBubbleViewTest, PressButtonTwice) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  EXPECT_EQ(GetInkDropState(1), views::InkDropState::HIDDEN);
  ClickApp(1);
  EXPECT_EQ(GetInkDropState(1), views::InkDropState::ACTIVATED);
  // Move mouse to prevent the mouse events being interpreted as a double-click.
  event_generator_->MoveMouseBy(10, 0);
  event_generator_->ClickLeftButton();
  EXPECT_EQ(GetInkDropState(1), views::InkDropState::ACTIVATED);
}

// Check that a non nullptr WebContents() has been created and observed.
TEST_F(IntentPickerBubbleViewTest, WebContentsTiedToBubble) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  EXPECT_TRUE(bubble_->web_contents());

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  EXPECT_TRUE(bubble_->web_contents());
}

// Check that that the correct window title is shown.
TEST_F(IntentPickerBubbleViewTest, WindowTitle) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH),
            bubble_->GetWindowTitle());

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kClickToCall,
                   /*initiating_origin=*/std::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL),
            bubble_->GetWindowTitle());
}

// Check that that the correct button labels are used.
TEST_F(IntentPickerBubbleViewTest, ButtonLabels) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN),
            bubble_->GetDialogButtonLabel(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME),
      bubble_->GetDialogButtonLabel(ui::mojom::DialogButton::kCancel));

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kClickToCall,
                   /*initiating_origin=*/std::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_CALL_BUTTON_LABEL),
            bubble_->GetDialogButtonLabel(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME),
      bubble_->GetDialogButtonLabel(ui::mojom::DialogButton::kCancel));
}

TEST_F(IntentPickerBubbleViewTest, InitiatingOriginView) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  const int children_without_origin = bubble_->children().size();

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   url::Origin::Create(GURL("https://example.com")));
  const int children_with_origin = bubble_->children().size();
  EXPECT_EQ(children_without_origin + 1, children_with_origin);

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   url::Origin::Create(GURL("http://www.google.com")));
  const int children_with_same_origin = bubble_->children().size();
  EXPECT_EQ(children_without_origin, children_with_same_origin);
}

enum class BubbleInterfaceType { kListView, kGridView };

class IntentPickerBubbleViewLayoutTest
    : public IntentPickerBubbleViewTest,
      public ::testing::WithParamInterface<BubbleInterfaceType> {
 public:
  IntentPickerBubbleViewLayoutTest() {
    if (GetParam() == BubbleInterfaceType::kGridView) {
      feature_list_.InitWithFeaturesAndParameters(
          apps::test::GetFeaturesToEnableLinkCapturingUX(), {});
    } else {
      feature_list_.InitWithFeatures(
          {}, apps::test::GetFeaturesToDisableLinkCapturingUX());
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(IntentPickerBubbleViewLayoutTest, RememberCheckbox) {
  AddApp(apps::PickerEntryType::kDevice, "device_id", "Android Phone");
  AddApp(apps::PickerEntryType::kWeb, "web_app_id", "Web App");
  AddApp(apps::PickerEntryType::kArc, "arc_app_id", "Arc App");

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  auto* checkbox = views::AsViewClass<views::Checkbox>(
      bubble_->GetViewByID(IntentPickerBubbleView::ViewId::kRememberCheckbox));

  // kDevice entries should not allow persistence.
  ClickApp(0);
  ASSERT_FALSE(checkbox->GetEnabled());

  // kWeb entries should allow persistence on CrOS. The checkbox does not pop up
  // on non-ChromeOS platforms, and persistence results in a no-op behavior.
  ClickApp(1);
  ASSERT_TRUE(checkbox->GetEnabled());

  // Other app types can be persisted.
  ClickApp(2);
  ASSERT_TRUE(checkbox->GetEnabled());
}

TEST_P(IntentPickerBubbleViewLayoutTest, AcceptDialog) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_1", "Web App");
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_2", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  ClickApp(1);
  bubble_->AcceptDialog();

  EXPECT_EQ(last_selected_launch_name_, "web_app_id_2");
  EXPECT_FALSE(last_selection_should_persist_);
  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::OPEN_APP);
}

TEST_P(IntentPickerBubbleViewLayoutTest, AcceptDialogWithRememberSelection) {
  AddApp(apps::PickerEntryType::kArc, "arc_app_id", "ARC App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  ClickApp(0);

  auto* checkbox = views::AsViewClass<views::Checkbox>(
      bubble_->GetViewByID(IntentPickerBubbleView::ViewId::kRememberCheckbox));
  checkbox->SetChecked(true);

  bubble_->AcceptDialog();

  EXPECT_EQ(last_selected_launch_name_, "arc_app_id");
  EXPECT_TRUE(last_selection_should_persist_);
  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::OPEN_APP);
}

TEST_P(IntentPickerBubbleViewLayoutTest, CancelDialog) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_1", "Web App");
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_2", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  ClickApp(1);
  bubble_->CancelDialog();

  EXPECT_EQ(last_selected_launch_name_, apps_util::kUseBrowserForLink);
  EXPECT_FALSE(last_selection_should_persist_);
  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::STAY_IN_CHROME);
}

TEST_P(IntentPickerBubbleViewLayoutTest, CloseDialog) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_1", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  bubble_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);

  ASSERT_EQ(last_close_reason_,
            apps::IntentPickerCloseReason::DIALOG_DEACTIVATED);
}

// TODO(crbug.com/40843230): Fix flakiness on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_KeyboardNavigation DISABLED_KeyboardNavigation
#else
#define MAYBE_KeyboardNavigation KeyboardNavigation
#endif
TEST_P(IntentPickerBubbleViewLayoutTest, MAYBE_KeyboardNavigation) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  ClickApp(0);
  GetButtonAtIndex(0)->RequestFocus();

  event_generator_->PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(bubble_->GetSelectedIndex(), 1u);
  event_generator_->PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_EQ(bubble_->GetSelectedIndex(), 0u);
  // Pressing up/left from the first item should wrap around to the last item.
  event_generator_->PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(bubble_->GetSelectedIndex(),
            bubble_->app_info_for_testing().size() - 1);
  // Pressing down/right from the last item should wrap around to the first
  // item.
  event_generator_->PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_EQ(bubble_->GetSelectedIndex(), 0u);
}

TEST_P(IntentPickerBubbleViewLayoutTest, DoubleClickToAccept) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  views::test::ButtonTestApi button(GetButtonAtIndex(0));

  button.NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::PointF(),
                                    gfx::PointF(), ui::EventTimeForNow(),
                                    ui::EF_NONE, ui::EF_NONE));
  button.NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::PointF(),
                                    gfx::PointF(), ui::EventTimeForNow(),
                                    ui::EF_IS_DOUBLE_CLICK, ui::EF_NONE));

  EXPECT_EQ(last_selected_launch_name_, "web_app_id");
  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::OPEN_APP);
}

INSTANTIATE_TEST_SUITE_P(All,
                         IntentPickerBubbleViewLayoutTest,
                         testing::Values(BubbleInterfaceType::kListView,
                                         BubbleInterfaceType::kGridView));

class IntentPickerBubbleViewGridLayoutTest : public IntentPickerBubbleViewTest {
 public:
  IntentPickerBubbleViewGridLayoutTest() {
    feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(), {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(IntentPickerBubbleViewGridLayoutTest, DefaultSelectionOneApp) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  ASSERT_EQ(bubble_->GetSelectedIndex(), 0u);
}

TEST_F(IntentPickerBubbleViewGridLayoutTest, AccessibilityCheckedStateChange) {
  ui::AXNodeData data;
  AddApp(apps::PickerEntryType::kWeb, "web_app_id", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);
  // In case of 1 app, 1st view will be selected
  bubble_->SelectDefaultItem();
  auto* view = GetButtonAtIndex(0);
  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  AddApp(apps::PickerEntryType::kWeb, "web_app_id", "Web App");
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  data = ui::AXNodeData();
  view = GetButtonAtIndex(1);
  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);
}

TEST_F(IntentPickerBubbleViewGridLayoutTest, DefaultSelectionTwoApps) {
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_1", "Web App");
  AddApp(apps::PickerEntryType::kWeb, "web_app_id_2", "Web App");

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  ASSERT_FALSE(bubble_->GetSelectedIndex().has_value());
}

// TODO(crbug.com/40843230): Fix flakiness on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OpenWithReturnKey DISABLED_OpenWithReturnKey
#else
#define MAYBE_OpenWithReturnKey OpenWithReturnKey
#endif
TEST_F(IntentPickerBubbleViewGridLayoutTest, MAYBE_OpenWithReturnKey) {
  AddDefaultApps();
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   BubbleType::kLinkCapturing,
                   /*initiating_origin=*/std::nullopt);

  GetButtonAtIndex(0)->RequestFocus();
  EXPECT_TRUE(GetButtonAtIndex(0)->HasFocus());

  event_generator_->PressKey(ui::VKEY_RETURN, ui::EF_NONE);

  EXPECT_EQ(last_close_reason_, apps::IntentPickerCloseReason::OPEN_APP);
}
