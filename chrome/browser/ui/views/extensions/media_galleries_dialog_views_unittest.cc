// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"
#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/storage_monitor/storage_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/checkbox.h"

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
  gallery.display_name = base::ASCIIToUTF16("Display Name");
  return gallery;
}

}  // namespace

class MediaGalleriesDialogTest : public testing::Test {
 public:
  MediaGalleriesDialogTest() {}
  ~MediaGalleriesDialogTest() override {}
  void SetUp() override {
    std::vector<base::string16> headers;
    headers.push_back(base::string16());
    headers.push_back(base::ASCIIToUTF16("header2"));
    ON_CALL(controller_, GetSectionHeaders()).
        WillByDefault(Return(headers));
    EXPECT_CALL(controller_, GetSectionEntries(_)).
        Times(AnyNumber());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(&controller_);
  }

  NiceMock<MediaGalleriesDialogControllerMock>* controller() {
    return &controller_;
  }

 private:
  // TODO(gbillock): Get rid of this mock; make something specialized.
  NiceMock<MediaGalleriesDialogControllerMock> controller_;
  ChromeTestViewsDelegate test_views_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogTest);
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
  MediaGalleriesDialogController::Entries attached_permissions;
  attached_permissions.push_back(
      MediaGalleriesDialogController::Entry(MakePrefInfoForTesting(1), true));
  EXPECT_CALL(*controller(), GetSectionEntries(0)).
      WillRepeatedly(Return(attached_permissions));

  MediaGalleriesDialogViews dialog(controller());
  EXPECT_EQ(1U, dialog.checkbox_map_.size());
  views::Checkbox* checkbox = dialog.checkbox_map_[1]->checkbox();
  EXPECT_TRUE(checkbox->GetChecked());

  ui::KeyEvent dummy_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  EXPECT_CALL(*controller(), DidToggleEntry(1, false));
  checkbox->SetChecked(false);
  dialog.ButtonPressed(checkbox, dummy_event);

  EXPECT_CALL(*controller(), DidToggleEntry(1, true));
  checkbox->SetChecked(true);
  dialog.ButtonPressed(checkbox, dummy_event);
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
