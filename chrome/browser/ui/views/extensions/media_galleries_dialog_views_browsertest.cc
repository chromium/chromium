// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/storage_monitor/storage_info.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/checkbox.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

MediaGalleryPrefInfo MakePrefInfo(
    MediaGalleryPrefId id,
    storage_monitor::StorageInfo::Type storage_type) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = id;
  gallery.device_id = storage_monitor::StorageInfo::MakeDeviceId(
      storage_type, base::NumberToString(id));
  gallery.display_name = u"Display Name";
  return gallery;
}

}  // namespace

class MediaGalleriesInteractiveDialogTest : public DialogBrowserTest {
 public:
  MediaGalleriesInteractiveDialogTest() {}

  MediaGalleriesInteractiveDialogTest(
      const MediaGalleriesInteractiveDialogTest&) = delete;
  MediaGalleriesInteractiveDialogTest& operator=(
      const MediaGalleriesInteractiveDialogTest&) = delete;

  ~MediaGalleriesInteractiveDialogTest() override {}

  void PreRunTestOnMainThread() override {
    DialogBrowserTest::PreRunTestOnMainThread();
    const GURL about_blank(url::kAboutBlankURL);
    content::WebContents* content =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(content);
    EXPECT_CALL(controller_, WebContents())
        .WillRepeatedly(testing::Return(content));
    ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
        browser(), about_blank, 1, WindowOpenDisposition::CURRENT_TAB,
        IsInteractiveUi() ? ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP : 0);
  }

  void ShowUi(const std::string& name) override {
    std::vector<std::u16string> headers = {std::u16string(), u"header2"};
    MediaGalleriesDialogController::Entries attached_permissions = {
        MediaGalleriesDialogController::Entry(
            MakePrefInfo(1, storage_monitor::StorageInfo::FIXED_MASS_STORAGE),
            true),
        MediaGalleriesDialogController::Entry(
            MakePrefInfo(
                2,
                storage_monitor::StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM),
            false)};
    ON_CALL(controller_, GetSectionHeaders())
        .WillByDefault(testing::Return(headers));
    EXPECT_CALL(controller_, GetSectionEntries(testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(controller_, GetSectionEntries(0))
        .WillRepeatedly(testing::Return(attached_permissions));

    dialog_ = std::make_unique<MediaGalleriesDialogViews>(&controller_);
  }

 private:
  testing::NiceMock<MediaGalleriesDialogControllerMock> controller_;
  std::unique_ptr<MediaGalleriesDialogViews> dialog_;
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesInteractiveDialogTest,
                       InvokeUi_DisplayDialog) {
  ShowAndVerifyUi();
}
