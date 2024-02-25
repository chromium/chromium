// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_tab_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views_test_api.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

using testing::Return;
using testing::ReturnRef;

const content::DesktopMediaID kDesktopMediaID0(
    content::DesktopMediaID::Type::TYPE_WEB_CONTENTS,
    0);
const content::DesktopMediaID kDesktopMediaID1(
    content::DesktopMediaID::Type::TYPE_WEB_CONTENTS,
    1);
const std::u16string kSourceName0 = u"source_0";
const std::u16string kSourceName1 = u"source_1";
const int kMaxPreviewTitleLength = 500;

class DesktopMediaTabListTest : public testing::Test {
 public:
  DesktopMediaTabListTest() {
    picker_views_ = std::make_unique<DesktopMediaPickerViews>();

    const std::u16string kAppName = u"foo";
    DesktopMediaPicker::Params picker_params{
        DesktopMediaPicker::Params::RequestSource::kUnknown};
    picker_params.context = test_helper_.GetContext();
    picker_params.app_name = kAppName;
    picker_params.target_name = kAppName;
    picker_params.request_audio = true;

    std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
    source_lists.push_back(std::make_unique<FakeDesktopMediaList>(
        DesktopMediaList::Type::kWebContents));
    media_list_ = static_cast<FakeDesktopMediaList*>(source_lists.back().get());

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "DesktopMediaPickerDialogView");

    picker_views_->Show(picker_params, std::move(source_lists),
                        base::BindOnce([](content::DesktopMediaID id) {}));
    test_api_.set_picker(picker_views_.get());

    tab_list_ =
        static_cast<DesktopMediaTabList*>(test_api_.GetSelectedListView());
    list_ = tab_list_->table_;
    preview_ = tab_list_->preview_;
    preview_label_ = tab_list_->preview_label_;

    widget_destroyed_waiter_ =
        std::make_unique<views::test::WidgetDestroyedWaiter>(
            waiter.WaitIfNeededAndGet());

    SkBitmap bitmap0;
    bitmap0.allocN32Pixels(16, 14);
    bitmap0.eraseColor(SK_ColorYELLOW);
    preview_0_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap0);
    media_list_->AddSourceByFullMediaID(kDesktopMediaID0);
    media_list_->SetSourceName(0, kSourceName0);
    media_list_->SetSourcePreview(0, preview_0_);

    SkBitmap bitmap1;
    bitmap1.allocN32Pixels(16, 14);
    bitmap1.eraseColor(SK_ColorRED);
    preview_1_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap1);
    media_list_->AddSourceByFullMediaID(kDesktopMediaID1);
    media_list_->SetSourceName(1, kSourceName1);
    media_list_->SetSourcePreview(1, preview_1_);
  }

  ~DesktopMediaTabListTest() override = default;

  void TearDown() override {
    if (GetPickerDialogView()) {
      GetPickerDialogView()->GetWidget()->CloseNow();
    }
    widget_destroyed_waiter_->Wait();
  }

  DesktopMediaPickerDialogView* GetPickerDialogView() const {
    return picker_views_->GetDialogViewForTesting();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  views::ScopedViewsTestHelper test_helper_{
      std::make_unique<ChromeTestViewsDelegate<>>()};
  raw_ptr<FakeDesktopMediaList, DanglingUntriaged> media_list_;
  std::unique_ptr<DesktopMediaPickerViews> picker_views_;
  DesktopMediaPickerViewsTestApi test_api_;
  raw_ptr<DesktopMediaTabList, DanglingUntriaged> tab_list_;
  raw_ptr<views::ImageView, DanglingUntriaged> preview_;
  raw_ptr<views::TableView, DanglingUntriaged> list_;
  raw_ptr<views::Label, DanglingUntriaged> preview_label_;
  std::unique_ptr<views::test::WidgetDestroyedWaiter> widget_destroyed_waiter_;

  gfx::ImageSkia preview_0_;
  gfx::ImageSkia preview_1_;
};

TEST_F(DesktopMediaTabListTest, InitialSelection) {
  EXPECT_TRUE(preview_label_->GetText().empty());

  test_api_.PressMouseOnSourceAtIndex(0);

  EXPECT_EQ(preview_label_->GetText(), kSourceName0);
}

TEST_F(DesktopMediaTabListTest, TitleUpdatedIfTitleOfSelectedTabChanges) {
  test_api_.PressMouseOnSourceAtIndex(0);

  ASSERT_EQ(preview_label_->GetText(), kSourceName0);

  std::u16string new_name = u"new_name";
  media_list_->SetSourceName(0, new_name);

  // Label should have been updated.
  EXPECT_EQ(preview_label_->GetText(), new_name);
}

TEST_F(DesktopMediaTabListTest, SelectedSourceHasPreview) {
  test_api_.PressMouseOnSourceAtIndex(0);

  EXPECT_TRUE(preview_->GetImage().BackedBySameObjectAs(preview_0_));
}

TEST_F(DesktopMediaTabListTest, UpdatedPreview) {
  test_api_.PressMouseOnSourceAtIndex(0);

  SkBitmap bitmap_blue;
  bitmap_blue.allocN32Pixels(16, 14);
  bitmap_blue.eraseColor(SK_ColorBLUE);
  gfx::ImageSkia new_preview = gfx::ImageSkia::CreateFrom1xBitmap(bitmap_blue);

  EXPECT_FALSE(preview_->GetImage().BackedBySameObjectAs(new_preview));

  media_list_->SetSourcePreview(0, new_preview);

  EXPECT_TRUE(preview_->GetImage().BackedBySameObjectAs(new_preview));
}

// crbug.com/1284150: flaky on Lacros
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_IgnorePreviewUpdatesForUnselectedSource \
  DISABLED_IgnorePreviewUpdatesForUnselectedSource
#else
#define MAYBE_IgnorePreviewUpdatesForUnselectedSource \
  IgnorePreviewUpdatesForUnselectedSource
#endif
TEST_F(DesktopMediaTabListTest, MAYBE_IgnorePreviewUpdatesForUnselectedSource) {
  test_api_.PressMouseOnSourceAtIndex(0);

  // Let the tab list know that the non-selected source #1 has a new preview.
  media_list_->SetSourcePreview(1, gfx::ImageSkia());

  // Preview image is unchanged.
  EXPECT_TRUE(preview_->GetImage().BackedBySameObjectAs(preview_0_));
}

TEST_F(DesktopMediaTabListTest, PreviewedSourceChange) {
  test_api_.PressMouseOnSourceAtIndex(0);

  EXPECT_TRUE(preview_->GetImage().BackedBySameObjectAs(preview_0_));

  test_api_.PressMouseOnSourceAtIndex(1);

  EXPECT_FALSE(preview_->GetImage().BackedBySameObjectAs(preview_0_));
  EXPECT_TRUE(preview_->GetImage().BackedBySameObjectAs(preview_1_));
}

TEST_F(DesktopMediaTabListTest, LongPageTitle) {
  std::u16string long_title(kMaxPreviewTitleLength + 1, 'a');
  media_list_->SetSourceName(0, long_title);

  std::u16string short_title(kMaxPreviewTitleLength, 'a');

  // Select source #0 now that it has a long title.
  test_api_.PressMouseOnSourceAtIndex(0);

  EXPECT_EQ(preview_label_->GetText(), short_title);

  // Also fire an OnSourceNameChanged event for the selected source #0.
  media_list_->SetSourceName(0, long_title);

  EXPECT_EQ(preview_label_->GetText(), short_title);
}
