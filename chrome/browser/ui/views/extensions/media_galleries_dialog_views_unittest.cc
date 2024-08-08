// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/storage_monitor/storage_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/test/button_test_api.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnPointee;

namespace {

MediaGalleryPrefInfo MakePrefInfoForTesting(MediaGalleryPrefId id) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = id;
  gallery.device_id = storage_monitor::StorageInfo::MakeDeviceId(
      storage_monitor::StorageInfo::FIXED_MASS_STORAGE,
      base::NumberToString(id));
  gallery.display_name = u"Display Name";
  return gallery;
}

}  // namespace

class MediaGalleriesDialogTest : public ChromeViewsTestBase {
 public:
  MediaGalleriesDialogTest() {}

  MediaGalleriesDialogTest(const MediaGalleriesDialogTest&) = delete;
  MediaGalleriesDialogTest& operator=(const MediaGalleriesDialogTest&) = delete;

  ~MediaGalleriesDialogTest() override {}
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    std::vector<std::u16string> headers;
    headers.push_back(std::u16string());
    headers.push_back(u"header2");
    ON_CALL(controller_, GetSectionHeaders()).
        WillByDefault(Return(headers));
    EXPECT_CALL(controller_, GetSectionEntries(_)).
        Times(AnyNumber());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(&controller_);
    ChromeViewsTestBase::TearDown();
  }

  views::Widget::InitParams CreateParams(
      views::Widget::InitParams::Ownership ownership,
      views::Widget::InitParams::Type type) override {
    // This relies on the setup done in the ToggleCheckboxes test below.
    auto dialog = std::make_unique<MediaGalleriesDialogViews>(controller());
    dialog->SetModalType(ui::mojom::ModalType::kWindow);
    EXPECT_EQ(1U, dialog->checkbox_map_.size());
    checkbox_ = dialog->checkbox_map_[1]->checkbox();
    EXPECT_TRUE(checkbox_->GetChecked());

    views::Widget::InitParams params =
        ChromeViewsTestBase::CreateParams(ownership, type);
    params.delegate = dialog.release();
    params.delegate->SetOwnedByWidget(true);
    return params;
  }

  NiceMock<MediaGalleriesDialogControllerMock>* controller() {
    return &controller_;
  }

  views::Checkbox* checkbox() { return checkbox_; }

 private:
  // TODO(gbillock): Get rid of this mock; make something specialized.
  NiceMock<MediaGalleriesDialogControllerMock> controller_;

  raw_ptr<views::Checkbox, DanglingUntriaged> checkbox_ = nullptr;
};

// Tests that checkboxes are initialized according to the contents of
// permissions in the registry.
TEST_F(MediaGalleriesDialogTest, InitializeCheckboxes) {
  MediaGalleriesDialogController::Entries attached_permissions;
  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(MakePrefInfoForTesting(1), true));
  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(MakePrefInfoForTesting(2), false));
  EXPECT_CALL(*controller(), GetSectionEntries(0)).
      WillRepeatedly(Return(attached_permissions));

  MediaGalleriesDialogViews dialog(controller());
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  MediaGalleryCheckboxView* checkbox_view1 = dialog.checkbox_map_[1];
  EXPECT_TRUE(checkbox_view1->checkbox()->GetChecked());

  MediaGalleryCheckboxView* checkbox_view2 = dialog.checkbox_map_[2];
  EXPECT_FALSE(checkbox_view2->checkbox()->GetChecked());
}

// Tests that toggling checkboxes updates the controller.
TEST_F(MediaGalleriesDialogTest, ToggleCheckboxes) {
  // Setup necessary for the expectations in CreateParams() above to pass.
  MediaGalleriesDialogController::Entries attached_permissions;
  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(MakePrefInfoForTesting(1), true));
  EXPECT_CALL(*controller(), GetSectionEntries(0)).
      WillRepeatedly(Return(attached_permissions));

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  EXPECT_CALL(*controller(), DidToggleEntry(1, false));
  views::test::ButtonTestApi test_api(checkbox());
  ui::KeyEvent dummy_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  test_api.NotifyClick(dummy_event);  // Toggles to unchecked before notifying.

  EXPECT_CALL(*controller(), DidToggleEntry(1, true));
  test_api.NotifyClick(dummy_event);  // Toggles to checked before notifying.

  widget->CloseNow();
}

// Tests that UpdateGallery will add a new checkbox, but only if it refers to
// a gallery that the dialog hasn't seen before.
TEST_F(MediaGalleriesDialogTest, UpdateAdds) {
  MediaGalleriesDialogViews dialog(controller());

  MediaGalleriesDialogController::Entries attached_permissions;
  EXPECT_CALL(*controller(), GetSectionEntries(0)).
      WillRepeatedly(ReturnPointee(&attached_permissions));

  EXPECT_TRUE(dialog.checkbox_map_.empty());

  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(gallery1, true));
  dialog.UpdateGalleries();
  EXPECT_EQ(1U, dialog.checkbox_map_.size());

  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(gallery2, true));
  dialog.UpdateGalleries();
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(gallery2, false));
  dialog.UpdateGalleries();
  EXPECT_EQ(2U, dialog.checkbox_map_.size());
}

TEST_F(MediaGalleriesDialogTest, ForgetDeletes) {
  MediaGalleriesDialogViews dialog(controller());

  MediaGalleriesDialogController::Entries attached_permissions;
  EXPECT_CALL(*controller(), GetSectionEntries(0)).
      WillRepeatedly(ReturnPointee(&attached_permissions));

  EXPECT_TRUE(dialog.checkbox_map_.empty());

  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(gallery1, true));
  dialog.UpdateGalleries();
  EXPECT_EQ(1U, dialog.checkbox_map_.size());

  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(gallery2, true));
  dialog.UpdateGalleries();
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  attached_permissions.pop_back();
  dialog.UpdateGalleries();
  EXPECT_EQ(1U, dialog.checkbox_map_.size());
}
