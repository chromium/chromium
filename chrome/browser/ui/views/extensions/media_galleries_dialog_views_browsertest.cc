// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"
#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/storage_monitor/storage_info.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/checkbox.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

MediaGalleryPrefInfo MakePrefInfo(MediaGalleryPrefId id) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = id;
  gallery.device_id = storage_monitor::StorageInfo::MakeDeviceId(
      storage_monitor::StorageInfo::FIXED_MASS_STORAGE,
      base::NumberToString(id));
  gallery.display_name = u"Display Name";
  return gallery;
}

}  // namespace

class MediaGalleriesInteractiveDialogTest : public DialogBrowserTest {
 public:
  MediaGalleriesInteractiveDialogTest() {}
  ~MediaGalleriesInteractiveDialogTest() override {}

  void PreRunTestOnMainThread() override {
    DialogBrowserTest::PreRunTestOnMainThread();
    const GURL about_blank(url::kAboutBlankURL);
    content::WebContents* content = browser()->OpenURL(content::OpenURLParams(
        about_blank, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false));
    EXPECT_CALL(controller_, WebContents())
        .WillRepeatedly(testing::Return(content));
    content::TestNavigationManager manager(content, about_blank);
    manager.WaitForNavigationFinished();
  }

  void ShowUi(const std::string& name) override {
    std::vector<std::u16string> headers = {std::u16string(), u"header2"};
    MediaGalleriesDialogController::Entries attached_permissions = {
        MediaGalleriesDialogController::Entry(MakePrefInfo(1), true),
        MediaGalleriesDialogController::Entry(MakePrefInfo(2), false)};
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

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesInteractiveDialogTest);
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesInteractiveDialogTest,
                       InvokeUi_DisplayDialog) {
  ShowAndVerifyUi();
}
