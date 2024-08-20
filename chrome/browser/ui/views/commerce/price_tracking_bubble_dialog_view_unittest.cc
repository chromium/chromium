// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace {
const char kTestURL[] = "http://www.google.com";
const char16_t kTestBookmarkTitle[] = u"Bookmark title";
}  // namespace

class PriceTrackingBubbleDialogViewUnitTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    anchor_widget_ =
        views::UniqueWidgetPtr(std::make_unique<ChromeTestWidget>());
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
    widget_params.context = GetContext();
    anchor_widget_->Init(std::move(widget_params));

    bubble_coordinator_ = std::make_unique<PriceTrackingBubbleCoordinator>(
        anchor_widget_->GetContentsView());

    SetUpDependencies();
  }

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    if (bubble_coordinator_->GetBubble()) {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          bubble_coordinator_->GetBubble()->GetWidget());
      bubble_coordinator_->GetBubble()->GetWidget()->Close();
      destroyed_waiter.Wait();
    }

    anchor_widget_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
            {TestingProfile::TestingFactory{
                BookmarkModelFactory::GetInstance(),
                BookmarkModelFactory::GetDefaultFactory()}});
  }

  void CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type type) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bubble_coordinator_->Show(
        browser()->tab_strip_model()->GetWebContentsAt(0), profile(),
        GURL(kTestURL),
        ui::ImageModel::FromImage(
            gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap))),
        Callback().Get(), OnDialogClosingCallback().Get(), type);
  }

  base::MockCallback<PriceTrackingBubbleDialogView::OnTrackPriceCallback>&
  Callback() {
    return callback_;
  }

  base::MockCallback<base::OnceClosure>& OnDialogClosingCallback() {
    return on_dialog_closing_callback_;
  }

  PriceTrackingBubbleCoordinator* BubbleCoordinator() {
    return bubble_coordinator_.get();
  }

 protected:
  virtual void SetUpDependencies() {
    bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);

    // Pretend sync is on for bookmarks, required for price tracking.
    LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(profile())
        ->SetIsTrackingMetadataForTesting();
  }

  raw_ptr<bookmarks::BookmarkModel, DanglingUntriaged> bookmark_model_;

 private:
  views::UniqueWidgetPtr anchor_widget_;
  base::MockCallback<PriceTrackingBubbleDialogView::OnTrackPriceCallback>
      callback_;
  std::unique_ptr<PriceTrackingBubbleCoordinator> bubble_coordinator_;
  base::MockCallback<base::OnceClosure> on_dialog_closing_callback_;
};

class PriceTrackingBubbleDialogViewLayoutUnitTest
    : public PriceTrackingBubbleDialogViewUnitTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool BookmarkWasCreated() { return GetParam(); }

  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    if (info.param) {
      return "TrackBookmarkedPage";
    } else {
      return "TrackNonBookmarkedPage";
    }
  }

  std::u16string GetFolderName() {
    if (BookmarkWasCreated()) {
      return bookmark_folder_name_;
    } else {
      return u"";
    }
  }

 protected:
  void SetUpDependencies() override {
    PriceTrackingBubbleDialogViewUnitTest::SetUpDependencies();

    EXPECT_FALSE(
        bookmarks::IsBookmarkedByUser(bookmark_model_, GURL(kTestURL)));

    if (BookmarkWasCreated()) {
      auto* node = bookmarks::AddIfNotBookmarked(
          bookmark_model_, GURL(kTestURL), kTestBookmarkTitle);
      EXPECT_TRUE(
          bookmarks::IsBookmarkedByUser(bookmark_model_, GURL(kTestURL)));
      bookmark_folder_name_ = node->parent()->GetTitle();
    }
  }

 private:
  std::u16string bookmark_folder_name_;
};

TEST_P(PriceTrackingBubbleDialogViewLayoutUnitTest, FUEBubble) {
  CreateBubbleViewAndShow(
      PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  EXPECT_EQ(bubble->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_OMNIBOX_TRACK_PRICE_DIALOG_TITLE_FIRST_RUN));

  EXPECT_TRUE(bubble->GetBodyLabelForTesting());
  EXPECT_EQ(bubble->GetBodyLabelForTesting()->GetText(),
            l10n_util::GetStringFUTF16(
                IDS_OMNIBOX_TRACK_PRICE_DIALOG_DESCRIPTION_FIRST_RUN,
                GetFolderName()));

  EXPECT_EQ(
      bubble->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
      l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE_DIALOG_ACTION_BUTTON));
  EXPECT_EQ(
      bubble->GetDialogButtonLabel(ui::mojom::DialogButton::kCancel),
      l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE_DIALOG_CANCEL_BUTTON));
}

TEST_P(PriceTrackingBubbleDialogViewLayoutUnitTest, NormalBubble) {
  // Price tracking can't happen if the bookmark wasn't created.
  if (!BookmarkWasCreated()) {
    return;
  }

  CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  EXPECT_EQ(bubble->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE_DIALOG_TITLE));

  EXPECT_TRUE(bubble->GetBodyLabelForTesting());
  std::u16string expected_label =
      l10n_util::GetStringUTF16(IDS_PRICE_TRACKING_SAVE_DESCRIPTION);
  std::u16string expected_save_label = l10n_util::GetStringFUTF16(
      IDS_PRICE_TRACKING_SAVE_LOCATION, GetFolderName());
  EXPECT_TRUE(bubble->GetBodyLabelForTesting()->GetText().find(
                  expected_label) != std::u16string::npos);
  EXPECT_TRUE(bubble->GetBodyLabelForTesting()->GetText().find(
                  expected_save_label) != std::u16string::npos);
  EXPECT_TRUE(bubble->GetBodyLabelForTesting()->GetFirstLinkForTesting());

  EXPECT_EQ(bubble->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
            l10n_util::GetStringUTF16(
                IDS_OMNIBOX_TRACKING_PRICE_DIALOG_ACTION_BUTTON));
  EXPECT_EQ(bubble->GetDialogButtonLabel(ui::mojom::DialogButton::kCancel),
            l10n_util::GetStringUTF16(
                IDS_OMNIBOX_TRACKING_PRICE_DIALOG_UNTRACK_BUTTON));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PriceTrackingBubbleDialogViewLayoutUnitTest,
    ::testing::Values(true, false),
    &PriceTrackingBubbleDialogViewLayoutUnitTest::DescribeParams);

class PriceTrackingBubbleDialogViewActionUnitTest
    : public PriceTrackingBubbleDialogViewUnitTest {
 protected:
  void SetUpDependencies() override {
    PriceTrackingBubbleDialogViewUnitTest::SetUpDependencies();
    bookmarks::AddIfNotBookmarked(bookmark_model_, GURL(kTestURL),
                                  kTestBookmarkTitle);
  }
};

TEST_F(PriceTrackingBubbleDialogViewActionUnitTest, AcceptFUEBubble) {
  CreateBubbleViewAndShow(
      PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);
  EXPECT_CALL(Callback(), Run(true));
  EXPECT_CALL(OnDialogClosingCallback(), Run());
  bubble->Accept();
}

TEST_F(PriceTrackingBubbleDialogViewActionUnitTest, CancelNormalBubble) {
  CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);
  EXPECT_CALL(Callback(), Run(false));
  EXPECT_CALL(OnDialogClosingCallback(), Run());
  bubble->Cancel();
}

TEST_F(PriceTrackingBubbleDialogViewActionUnitTest,
       ClickLinkInTheNormalBubble) {
  CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  auto bookmark_editor_waiter = views::NamedWidgetShownWaiter(
      views::test::AnyWidgetTestPasskey{}, BookmarkEditorView::kViewClassName);

  EXPECT_CALL(OnDialogClosingCallback(), Run());
  bubble->GetBodyLabelForTesting()->ClickFirstLinkForTesting();
  EXPECT_TRUE(bookmark_editor_waiter.WaitIfNeededAndGet());

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(BubbleCoordinator()->GetBubble());
}
