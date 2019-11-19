// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/resources/grit/views_resources.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/intent_helper/arc_intent_picker_app_fetcher.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#endif

using AppInfo = apps::IntentPickerAppInfo;
using content::WebContents;
using content::OpenURLParams;
using content::Referrer;

// There is logic inside IntentPickerBubbleView that filters out the intent
// helper by checking IsIntentHelperPackage() on them. That logic is
// ChromeOS-only, so for this unit test to match the behavior of
// IntentPickerBubbleView on non-ChromeOS platforms, if needs to not filter any
// packages.
#if defined(OS_CHROMEOS)
const char* kArcIntentHelperPackageName =
    arc::ArcIntentHelperBridge::kArcIntentHelperPackageName;
bool (*IsIntentHelperPackage)(const std::string&) =
    arc::ArcIntentHelperBridge::IsIntentHelperPackage;
#else
static const char kArcIntentHelperPackageName[] = "unused_intent_helper";
bool IsIntentHelperPackage(const std::string& package_name) {
  return false;
}
#endif

class IntentPickerBubbleViewTest : public BrowserWithTestWindowTest {
 public:
  IntentPickerBubbleViewTest() = default;

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    bubble_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void CreateBubbleView(bool use_icons,
                        bool show_stay_in_chrome,
                        PageActionIconType icon_type,
                        const base::Optional<url::Origin>& initiating_origin) {
    anchor_view_ = std::make_unique<views::View>();

    // Pushing a couple of fake apps just to check they are created on the UI.
    app_info_.emplace_back(apps::PickerEntryType::kArc, gfx::Image(),
                           "package_1", "dank app 1");
    app_info_.emplace_back(apps::PickerEntryType::kArc, gfx::Image(),
                           "package_2", "dank_app_2");
    // Also adding the corresponding Chrome's package name on ARC, even if this
    // is given to the picker UI as input it should be ignored.
    app_info_.emplace_back(apps::PickerEntryType::kArc, gfx::Image(),
                           kArcIntentHelperPackageName, "legit_chrome");

    if (use_icons)
      FillAppListWithDummyIcons();

    // We create |web_contents| since the Bubble UI has an Observer that
    // depends on this, otherwise it wouldn't work.
    GURL url("http://www.google.com");
    WebContents* web_contents = browser()->OpenURL(
        OpenURLParams(url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false));
    CommitPendingLoad(&web_contents->GetController());

    std::vector<AppInfo> app_info;

    // AppInfo is move only. Manually create a new app_info array to pass into
    // the bubble constructor.
    for (const auto& app : app_info_) {
      app_info.emplace_back(app.type, app.icon, app.launch_name,
                            app.display_name);
    }

    bubble_ = IntentPickerBubbleView::CreateBubbleViewForTesting(
        anchor_view_.get(), /*icon_view=*/nullptr, icon_type,
        std::move(app_info), show_stay_in_chrome,
        /*show_remember_selection=*/true, initiating_origin,
        base::Bind(&IntentPickerBubbleViewTest::OnBubbleClosed,
                   base::Unretained(this)),
        web_contents);
  }

  void FillAppListWithDummyIcons() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::Image dummy_icon = rb.GetImageNamed(IDR_CLOSE);
    for (auto& app : app_info_)
      app.icon = dummy_icon;
  }

  // Dummy method to be called upon bubble closing.
  void OnBubbleClosed(const std::string& selected_app_package,
                      apps::PickerEntryType entry_type,
                      apps::IntentPickerCloseReason close_reason,
                      bool should_persist) {}

  std::unique_ptr<IntentPickerBubbleView> bubble_;
  std::unique_ptr<views::View> anchor_view_;
  std::vector<AppInfo> app_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IntentPickerBubbleViewTest);
};

// Verifies that we didn't set up an image for any LabelButton.
TEST_F(IntentPickerBubbleViewTest, NullIcons) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  size_t size = bubble_->GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    gfx::ImageSkia image = bubble_->GetAppImageForTesting(i);
    EXPECT_TRUE(image.isNull()) << i;
  }
}

// Verifies that all the icons contain a non-null icon.
TEST_F(IntentPickerBubbleViewTest, NonNullIcons) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  size_t size = bubble_->GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    gfx::ImageSkia image = bubble_->GetAppImageForTesting(i);
    EXPECT_FALSE(image.isNull()) << i;
  }
}

// Verifies that the bubble contains as many rows as |app_info_| with one
// exception, if the Chrome package is present on the input list it won't be
// shown to the user on the picker UI, so there could be a difference
// represented by |chrome_package_repetitions|.
TEST_F(IntentPickerBubbleViewTest, LabelsPtrVectorSize) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  size_t size = app_info_.size();
  size_t chrome_package_repetitions = 0;
  for (const AppInfo& app_info : app_info_) {
    if (IsIntentHelperPackage(app_info.launch_name))
      ++chrome_package_repetitions;
  }

  EXPECT_EQ(size, bubble_->GetScrollViewSize() + chrome_package_repetitions);
}

// Verifies the InkDrop state when creating a new bubble.
TEST_F(IntentPickerBubbleViewTest, VerifyStartingInkDrop) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  size_t size = bubble_->GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(bubble_->GetInkDropStateForTesting(i),
              views::InkDropState::HIDDEN);
  }
}

// Press each button at a time and make sure it goes to ACTIVATED state,
// followed by HIDDEN state after selecting other button.
TEST_F(IntentPickerBubbleViewTest, InkDropStateTransition) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  size_t size = bubble_->GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    bubble_->PressButtonForTesting((i + 1) % size, event);
    EXPECT_EQ(bubble_->GetInkDropStateForTesting(i),
              views::InkDropState::HIDDEN);
    EXPECT_EQ(bubble_->GetInkDropStateForTesting((i + 1) % size),
              views::InkDropState::ACTIVATED);
  }
}

// Arbitrary press the first button twice, check that the InkDropState remains
// the same.
TEST_F(IntentPickerBubbleViewTest, PressButtonTwice) {
  CreateBubbleView(/*use_icons=*/true, /*show_stay_in_chrome=*/true,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  EXPECT_EQ(bubble_->GetInkDropStateForTesting(0), views::InkDropState::HIDDEN);
  bubble_->PressButtonForTesting(0, event);
  EXPECT_EQ(bubble_->GetInkDropStateForTesting(0),
            views::InkDropState::ACTIVATED);
  bubble_->PressButtonForTesting(0, event);
  EXPECT_EQ(bubble_->GetInkDropStateForTesting(0),
            views::InkDropState::ACTIVATED);
}

// Check that none of the app candidates within the picker corresponds to the
// Chrome browser.
TEST_F(IntentPickerBubbleViewTest, ChromeNotInCandidates) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  size_t size = bubble_->GetScrollViewSize();
  for (size_t i = 0; i < size; ++i) {
    EXPECT_FALSE(
        IsIntentHelperPackage(bubble_->app_info_for_testing()[i].launch_name));
  }
}

// Check that a non nullptr WebContents() has been created and observed.
TEST_F(IntentPickerBubbleViewTest, WebContentsTiedToBubble) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  EXPECT_TRUE(bubble_->web_contents());

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/true,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  EXPECT_TRUE(bubble_->web_contents());
}

// Check that that the correct window title is shown.
TEST_F(IntentPickerBubbleViewTest, WindowTitle) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH),
            bubble_->GetWindowTitle());

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   PageActionIconType::kClickToCall,
                   /*initiating_origin=*/base::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL),
            bubble_->GetWindowTitle());
}

// Check that that the correct button labels are used.
TEST_F(IntentPickerBubbleViewTest, ButtonLabels) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN),
            bubble_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME),
      bubble_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   PageActionIconType::kClickToCall,
                   /*initiating_origin=*/base::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_CALL_BUTTON_LABEL),
            bubble_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME),
      bubble_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));
}

TEST_F(IntentPickerBubbleViewTest, InitiatingOriginView) {
  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   PageActionIconType::kIntentPicker,
                   /*initiating_origin=*/base::nullopt);
  const int children_without_origin = bubble_->children().size();

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   PageActionIconType::kIntentPicker,
                   url::Origin::Create(GURL("https://example.com")));
  const int children_with_origin = bubble_->children().size();
  EXPECT_EQ(children_without_origin + 1, children_with_origin);

  CreateBubbleView(/*use_icons=*/false, /*show_stay_in_chrome=*/false,
                   PageActionIconType::kIntentPicker,
                   url::Origin::Create(GURL("http://www.google.com")));
  const int children_with_same_origin = bubble_->children().size();
  EXPECT_EQ(children_without_origin, children_with_same_origin);
}
