// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_dialog.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include <variant>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/extensions/rich_radio_button.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/gurl.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

using DialogResult = SettingsOverriddenDialogController::DialogResult;

namespace {

struct DialogState {
  bool shown = false;
  std::optional<DialogResult> result;
};

// A dialog controller that updates the provided DialogState when the dialog
// is interacted with.
class TestDialogController : public SettingsOverriddenDialogController {
 public:
  explicit TestDialogController(
      DialogState* state,
      ShowParams show_params = ShowParams(u"Dialog Title",
                                          u"Dialog Body",
                                          /*icon=*/nullptr),
      image_fetcher::ImageFetcher* image_fetcher = nullptr)
      : state_(state),
        show_params_(std::move(show_params)),
        image_fetcher_(image_fetcher) {}
  TestDialogController(const TestDialogController&) = delete;
  TestDialogController& operator=(const TestDialogController&) = delete;
  ~TestDialogController() override = default;

 private:
  bool ShouldShow() override { return true; }
  ShowParams GetShowParams() override { return show_params_; }
  image_fetcher::ImageFetcher* GetImageFetcher() override {
    return image_fetcher_;
  }
  void OnDialogWillBeShown() override {
    EXPECT_FALSE(state_->shown)
        << "OnDialogWillBeShown() called more than once!";
    state_->shown = true;
  }
  void HandleDialogResult(DialogResult result) override {
    state_->result = result;
  }

  const raw_ptr<DialogState> state_;
  const ShowParams show_params_;
  const raw_ptr<image_fetcher::ImageFetcher> image_fetcher_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
SettingsOverriddenDialogController::ShowParams CreateExplicitChoiceShowParams(
    SettingsOverriddenDialogController::SettingOption::IconSource previous_icon,
    SettingsOverriddenDialogController::SettingOption::IconSource new_icon) {
  SettingsOverriddenDialogController::ShowParams show_params(
      u"Confirm search engine", u"An extension changed your search engine.",
      /*icon=*/nullptr);
  auto& previous = show_params.previous_setting.emplace();
  previous.text = u"Previous Search";
  previous.description = u"Your previous choice";
  previous.icon_source = std::move(previous_icon);

  auto& next = show_params.new_setting.emplace();
  next.text = u"New Search";
  next.description = u"Changed to this by extension";
  next.icon_source = std::move(new_icon);
  return show_params;
}

ui::ImageModel GetExpectedFallbackIcon() {
  return ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kGlobeIcon
                                        : vector_icons::kGlobeOldIcon,
      ui::kColorIcon, extension_misc::EXTENSION_ICON_SMALLISH);
}

// Compares the images, ensuring that varying types are compared appropriately.
bool IconsEqual(const ui::ImageModel& a, const ui::ImageModel& b) {
  if (a.IsImage() && b.IsImage()) {
    return gfx::test::AreBitmapsEqual(a.GetImage().AsBitmap(),
                                      b.GetImage().AsBitmap());
  }
  if (a.IsVectorIcon() && b.IsVectorIcon()) {
    return a.GetVectorIcon() == b.GetVectorIcon();
  }
  return false;
}

extensions::RichRadioButton* GetRadioButton(views::Widget* dialog,
                                            ui::ElementIdentifier id) {
  views::View* view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          id, views::ElementTrackerViews::GetContextForWidget(dialog));
  return static_cast<extensions::RichRadioButton*>(view);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace

class SettingsOverriddenDialogUnitTest : public ChromeViewsTestBase {
  // TODO(crbug.com/424013924): Remove browser dependency and enable test for
  // Desktop Android
 public:
  SettingsOverriddenDialogUnitTest() = default;
  SettingsOverriddenDialogUnitTest(const SettingsOverriddenDialogUnitTest&) =
      delete;
  const SettingsOverriddenDialogUnitTest& operator=(
      const SettingsOverriddenDialogUnitTest&) = delete;
  ~SettingsOverriddenDialogUnitTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    constrained_window::SetConstrainedWindowViewsClient(
        CreateChromeConstrainedWindowViewsClient());

    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_widget_->Show();
  }

  void TearDown() override {
    parent_widget_.reset();
    constrained_window::SetConstrainedWindowViewsClient(nullptr);
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* ShowDialog(DialogState* state,
                            std::unique_ptr<SettingsOverriddenDialogController>
                                controller = nullptr) {
    if (!controller) {
      controller = std::make_unique<TestDialogController>(state);
    }
    EXPECT_FALSE(state->shown);

    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        kExtensionSettingsOverriddenDialogName);
    extensions::ShowSettingsOverriddenDialog(std::move(controller),
                                             parent_widget_->GetNativeWindow());
    views::Widget* dialog = waiter.WaitIfNeededAndGet();
    EXPECT_TRUE(state->shown);

    return dialog;
  }

  gfx::NativeWindow parent_window() {
    return parent_widget_->GetNativeWindow();
  }

 private:
  std::unique_ptr<views::Widget> parent_widget_;
};

TEST_F(SettingsOverriddenDialogUnitTest, DialogResult_ChangeSettingsBack) {
  DialogState state;
  views::Widget* dialog = ShowDialog(&state);

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  dialog_waiter.Wait();

  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kChangeSettingsBack, state.result);
}

TEST_F(SettingsOverriddenDialogUnitTest, DialogResult_KeepNewSettings) {
  DialogState state;
  views::Widget* dialog = ShowDialog(&state);

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->widget_delegate()->AsDialogDelegate()->CancelDialog();
  dialog_waiter.Wait();

  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kKeepNewSettings, state.result);
}

TEST_F(SettingsOverriddenDialogUnitTest, DialogResult_DismissDialog) {
  DialogState state;
  views::Widget* dialog = ShowDialog(&state);

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->Close();
  dialog_waiter.Wait();

  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kDialogDismissed, state.result);
}

TEST_F(SettingsOverriddenDialogUnitTest, DialogResult_CloseParentWidget) {
  DialogState state;
  views::Widget* dialog = ShowDialog(&state);

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();

  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kDialogClosedWithoutUserAction, state.result);
}

// Showing a modal dialog can, on some platforms, result in the dialog widget
// being synchronously destroyed before Widget::Show() returns (for example if
// a nested run loop closes the parent window). Verify that the controller is
// notified that the dialog was shown, and that the result is delivered, even
// when the dialog is destroyed as soon as it is shown.
TEST_F(SettingsOverriddenDialogUnitTest, DialogDestroyedWhileBeingShown) {
  DialogState state;

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  observer.set_shown_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (widget->GetName() != kExtensionSettingsOverriddenDialogName) {
          return;
        }
        EXPECT_TRUE(state.shown);
        widget->CloseNow();
      }));

  extensions::ShowSettingsOverriddenDialog(
      std::make_unique<TestDialogController>(&state), parent_window());

  EXPECT_TRUE(state.shown);
  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kDialogClosedWithoutUserAction, state.result);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
class SettingsOverriddenExplicitChoiceDialogUnitTest
    : public SettingsOverriddenDialogUnitTest {
 protected:
  SettingsOverriddenExplicitChoiceDialogUnitTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kSearchEngineExplicitChoiceDialog);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SettingsOverriddenExplicitChoiceDialogUnitTest,
       NoIconUsesFallbackWithoutFetching) {
  testing::StrictMock<image_fetcher::MockImageFetcher> mock_fetcher;
  DialogState state;
  views::Widget* dialog = ShowDialog(
      &state,
      std::make_unique<TestDialogController>(
          &state,
          CreateExplicitChoiceShowParams(std::monostate{}, std::monostate{}),
          &mock_fetcher));

  extensions::RichRadioButton* previous_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogPreviousSettingButtonId);
  extensions::RichRadioButton* new_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogNewSettingButtonId);
  ASSERT_TRUE(previous_button);
  ASSERT_TRUE(new_button);

  const ui::ImageModel fallback = GetExpectedFallbackIcon();
  EXPECT_TRUE(IconsEqual(fallback, previous_button->GetImageForTesting()));
  EXPECT_TRUE(IconsEqual(fallback, new_button->GetImageForTesting()));

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();
}

TEST_F(SettingsOverriddenExplicitChoiceDialogUnitTest,
       BuiltinResourceIdLoadsResizedIconSynchronously) {
  testing::StrictMock<image_fetcher::MockImageFetcher> mock_fetcher;
  DialogState state;
  views::Widget* dialog = ShowDialog(
      &state,
      std::make_unique<TestDialogController>(
          &state,
          CreateExplicitChoiceShowParams(IDR_DEFAULT_FAVICON, std::monostate{}),
          &mock_fetcher));

  extensions::RichRadioButton* previous_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogPreviousSettingButtonId);
  ASSERT_TRUE(previous_button);
  const ui::ImageModel icon = previous_button->GetImageForTesting();
  ASSERT_TRUE(icon.IsImage());
  EXPECT_EQ(gfx::Size(extension_misc::EXTENSION_ICON_SMALLISH,
                      extension_misc::EXTENSION_ICON_SMALLISH),
            icon.Size());

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();
}

TEST_F(SettingsOverriddenExplicitChoiceDialogUnitTest,
       AsyncFaviconFetchUpdatesBothButtonsInPlace) {
  const GURL kPreviousFaviconUrl("https://previous.example/favicon.ico");
  const GURL kNewFaviconUrl("https://new.example/favicon.ico");
  testing::StrictMock<image_fetcher::MockImageFetcher> mock_fetcher;
  image_fetcher::ImageFetcherCallback previous_callback;
  image_fetcher::ImageFetcherCallback new_callback;
  EXPECT_CALL(mock_fetcher, FetchImageAndData_(kPreviousFaviconUrl, testing::_,
                                               testing::_, testing::_))
      .WillOnce([&](const GURL&, image_fetcher::ImageDataFetcherCallback*,
                    image_fetcher::ImageFetcherCallback* callback,
                    image_fetcher::ImageFetcherParams) {
        previous_callback = std::move(*callback);
      });
  EXPECT_CALL(mock_fetcher, FetchImageAndData_(kNewFaviconUrl, testing::_,
                                               testing::_, testing::_))
      .WillOnce([&](const GURL&, image_fetcher::ImageDataFetcherCallback*,
                    image_fetcher::ImageFetcherCallback* callback,
                    image_fetcher::ImageFetcherParams) {
        new_callback = std::move(*callback);
      });

  DialogState state;
  views::Widget* dialog = ShowDialog(
      &state,
      std::make_unique<TestDialogController>(
          &state,
          CreateExplicitChoiceShowParams(kPreviousFaviconUrl, kNewFaviconUrl),
          &mock_fetcher));

  extensions::RichRadioButton* previous_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogPreviousSettingButtonId);
  extensions::RichRadioButton* new_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogNewSettingButtonId);
  ASSERT_TRUE(previous_button);
  ASSERT_TRUE(new_button);

  // Before the network fetches complete, both options render the fallback icon.
  const ui::ImageModel fallback = GetExpectedFallbackIcon();
  EXPECT_TRUE(IconsEqual(fallback, previous_button->GetImageForTesting()));
  EXPECT_TRUE(IconsEqual(fallback, new_button->GetImageForTesting()));

  // Deliver distinct oversized bitmaps and verify each option updates in place
  // to its own resized icon.
  ASSERT_TRUE(previous_callback);
  ASSERT_TRUE(new_callback);
  std::move(previous_callback)
      .Run(gfx::test::CreateImage(64, 64, SK_ColorRED),
           image_fetcher::RequestMetadata());
  std::move(new_callback)
      .Run(gfx::test::CreateImage(64, 64, SK_ColorGREEN),
           image_fetcher::RequestMetadata());

  const ui::ImageModel expected_previous_icon =
      ui::ImageModel::FromImage(gfx::test::CreateImage(
          extension_misc::EXTENSION_ICON_SMALLISH,
          extension_misc::EXTENSION_ICON_SMALLISH, SK_ColorRED));
  const ui::ImageModel expected_new_icon =
      ui::ImageModel::FromImage(gfx::test::CreateImage(
          extension_misc::EXTENSION_ICON_SMALLISH,
          extension_misc::EXTENSION_ICON_SMALLISH, SK_ColorGREEN));
  EXPECT_TRUE(IconsEqual(expected_previous_icon,
                         previous_button->GetImageForTesting()));
  EXPECT_TRUE(IconsEqual(expected_new_icon, new_button->GetImageForTesting()));

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();
}

TEST_F(SettingsOverriddenExplicitChoiceDialogUnitTest,
       NonHttpFaviconUrlUsesFallbackWithoutFetching) {
  testing::StrictMock<image_fetcher::MockImageFetcher> mock_fetcher;
  DialogState state;
  views::Widget* dialog =
      ShowDialog(&state, std::make_unique<TestDialogController>(
                             &state,
                             CreateExplicitChoiceShowParams(
                                 GURL("data:image/png;base64,AAAA"),
                                 GURL("chrome://favicon/https://example.com")),
                             &mock_fetcher));

  extensions::RichRadioButton* previous_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogPreviousSettingButtonId);
  extensions::RichRadioButton* new_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogNewSettingButtonId);
  ASSERT_TRUE(previous_button);
  ASSERT_TRUE(new_button);

  const ui::ImageModel fallback = GetExpectedFallbackIcon();
  EXPECT_TRUE(IconsEqual(fallback, previous_button->GetImageForTesting()));
  EXPECT_TRUE(IconsEqual(fallback, new_button->GetImageForTesting()));

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();
}

TEST_F(SettingsOverriddenExplicitChoiceDialogUnitTest,
       EmptyAsyncFaviconKeepsFallbackIcon) {
  const GURL kFaviconUrl("https://example.com/missing.ico");
  testing::StrictMock<image_fetcher::MockImageFetcher> mock_fetcher;
  image_fetcher::ImageFetcherCallback captured_callback;
  EXPECT_CALL(mock_fetcher, FetchImageAndData_(kFaviconUrl, testing::_,
                                               testing::_, testing::_))
      .WillOnce([&](const GURL&, image_fetcher::ImageDataFetcherCallback*,
                    image_fetcher::ImageFetcherCallback* callback,
                    image_fetcher::ImageFetcherParams) {
        captured_callback = std::move(*callback);
      });

  DialogState state;
  views::Widget* dialog = ShowDialog(
      &state,
      std::make_unique<TestDialogController>(
          &state, CreateExplicitChoiceShowParams(kFaviconUrl, std::monostate{}),
          &mock_fetcher));

  extensions::RichRadioButton* previous_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogPreviousSettingButtonId);
  ASSERT_TRUE(previous_button);
  ASSERT_TRUE(captured_callback);
  std::move(captured_callback)
      .Run(gfx::Image(), image_fetcher::RequestMetadata());

  EXPECT_TRUE(IconsEqual(GetExpectedFallbackIcon(),
                         previous_button->GetImageForTesting()));

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();
}

TEST_F(SettingsOverriddenExplicitChoiceDialogUnitTest,
       AsyncFaviconCompletingAfterDialogCloseIsSafe) {
  const GURL kFaviconUrl("https://example.com/slow.ico");
  testing::StrictMock<image_fetcher::MockImageFetcher> mock_fetcher;
  image_fetcher::ImageFetcherCallback captured_callback;
  EXPECT_CALL(mock_fetcher, FetchImageAndData_(kFaviconUrl, testing::_,
                                               testing::_, testing::_))
      .WillOnce([&](const GURL&, image_fetcher::ImageDataFetcherCallback*,
                    image_fetcher::ImageFetcherCallback* callback,
                    image_fetcher::ImageFetcherParams) {
        captured_callback = std::move(*callback);
      });

  DialogState state;
  views::Widget* dialog = ShowDialog(
      &state,
      std::make_unique<TestDialogController>(
          &state, CreateExplicitChoiceShowParams(kFaviconUrl, std::monostate{}),
          &mock_fetcher));

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();

  // Completing the fetch after the dialog widget has been destroyed must no-op.
  ASSERT_TRUE(captured_callback);
  std::move(captured_callback)
      .Run(gfx::test::CreateImage(32, 32, SK_ColorBLUE),
           image_fetcher::RequestMetadata());
}

TEST_F(SettingsOverriddenExplicitChoiceDialogUnitTest,
       NullImageFetcherKeepsFallbackIcon) {
  const GURL kFaviconUrl("https://example.com/favicon.ico");
  DialogState state;
  views::Widget* dialog = ShowDialog(
      &state,
      std::make_unique<TestDialogController>(
          &state, CreateExplicitChoiceShowParams(kFaviconUrl, std::monostate{}),
          /*image_fetcher=*/nullptr));

  extensions::RichRadioButton* previous_button =
      GetRadioButton(dialog, kSettingsOverriddenDialogPreviousSettingButtonId);
  ASSERT_TRUE(previous_button);
  EXPECT_TRUE(IconsEqual(GetExpectedFallbackIcon(),
                         previous_button->GetImageForTesting()));

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
