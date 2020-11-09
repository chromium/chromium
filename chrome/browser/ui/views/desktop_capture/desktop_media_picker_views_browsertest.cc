// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/window/dialog_delegate.h"

class DesktopMediaPickerViewsBrowserTest : public DialogBrowserTest {
 public:
  DesktopMediaPickerViewsBrowserTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    picker_ = std::make_unique<DesktopMediaPickerViews>();
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();

    std::vector<std::unique_ptr<DesktopMediaList>> sources;
    if (override_source_lists_.empty()) {
      sources = CreateDefaultSourceLists();
    } else {
      for (auto& source : override_source_lists_)
        sources.push_back(std::move(source));
    }

    std::vector<FakeDesktopMediaList*> source_lists;
    for (const auto& source : sources)
      source_lists.push_back(static_cast<FakeDesktopMediaList*>(source.get()));

    DesktopMediaPicker::Params picker_params;
    picker_params.web_contents = web_contents;
    picker_params.context = native_window;
    picker_params.app_name = base::ASCIIToUTF16("app_name");
    picker_params.target_name = base::ASCIIToUTF16("target_name");
    picker_params.request_audio = true;
    picker_->Show(picker_params, std::move(sources),
                  DesktopMediaPicker::DoneCallback());

    if (after_show_callback_)
      std::move(after_show_callback_).Run(source_lists);
  }

 protected:
  std::vector<std::unique_ptr<DesktopMediaList>> CreateDefaultSourceLists() {
    std::vector<std::unique_ptr<DesktopMediaList>> sources;
    for (auto type : {content::DesktopMediaID::TYPE_SCREEN,
                      content::DesktopMediaID::TYPE_WINDOW,
                      content::DesktopMediaID::TYPE_WEB_CONTENTS}) {
      sources.push_back(std::make_unique<FakeDesktopMediaList>(type));
    }
    return sources;
  }

  std::unique_ptr<DesktopMediaPickerViews> picker_;

  // If this list isn't filled in, a default list of source lists will be
  // created.
  std::vector<std::unique_ptr<FakeDesktopMediaList>> override_source_lists_;

  // This callback is called in ShowUi after the picker dialog has been shown.
  // This both more closely mirrors how this code behaves in production (where
  // the DesktopMediaList is filled asynchronously, so it starts off empty and
  // then becomes filled after the UI is showing) and allows InvokeUi-style
  // tests to update the UI state after showing it.
  base::OnceCallback<void(const std::vector<FakeDesktopMediaList*>&)>
      after_show_callback_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPickerViewsBrowserTest);
};

// Invokes a dialog that allows the user to select what view of their desktop
// they would like to share.
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest, InvokeUi_default) {
  after_show_callback_ =
      base::BindOnce([](const std::vector<FakeDesktopMediaList*>& sources) {
        sources[0]->AddSource(0);

        // Fill in a bit of test data for nicer UI screenshots :)
        sources[1]->AddSource(0);
        sources[1]->SetSourceName(0, base::ASCIIToUTF16("Warty Warthog"));
        sources[1]->AddSource(1);
        sources[1]->SetSourceName(1, base::ASCIIToUTF16("Hoary Hedgehog"));
        sources[1]->AddSource(2);
        sources[1]->SetSourceName(2, base::ASCIIToUTF16("Breezy Badger"));

        sources[2]->AddSource(0);
        sources[2]->SetSourceName(0, base::ASCIIToUTF16("Dapper Drake"));
        sources[2]->AddSource(1);
        sources[2]->SetSourceName(1, base::ASCIIToUTF16("Edgy Eft"));
        sources[2]->AddSource(2);
        sources[2]->SetSourceName(2, base::ASCIIToUTF16("Feisty Fawn"));
      });
  ShowAndVerifyUi();
}

// Show the picker UI with only one source type: TYPE_WEB_CONTENTS, aka the
// tab picker.
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest, InvokeUi_tabs) {
  after_show_callback_ =
      base::BindOnce([](const std::vector<FakeDesktopMediaList*>& sources) {
        sources[0]->AddSource(0);
        sources[0]->SetSourceName(0, base::ASCIIToUTF16("Dapper Drake"));
        sources[0]->AddSource(1);
        sources[0]->SetSourceName(1, base::ASCIIToUTF16("Edgy Eft"));
        sources[0]->AddSource(2);
        sources[0]->SetSourceName(2, base::ASCIIToUTF16("Feisty Fawn"));
      });
  override_source_lists_.push_back(std::make_unique<FakeDesktopMediaList>(
      content::DesktopMediaID::TYPE_WEB_CONTENTS));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest,
                       InitiallyFocusesDisabledOK) {
  ShowUi(std::string());
  views::DialogDelegate* dialog =
      picker_->GetDialogViewForTesting()->AsDialogDelegate();
  EXPECT_EQ(dialog->GetOkButton(),
            dialog->DialogDelegate::GetInitiallyFocusedView());
  EXPECT_FALSE(picker_->GetDialogViewForTesting()->IsDialogButtonEnabled(
      ui::DIALOG_BUTTON_OK));
}

// Validate that the dialog title changes to match the source type when there's
// only one source type present.
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest,
                       SingleSourceTypeChangesTitle) {
  override_source_lists_.push_back(std::make_unique<FakeDesktopMediaList>(
      content::DesktopMediaID::TYPE_WEB_CONTENTS));
  ShowUi(std::string());

  EXPECT_EQ(picker_->GetDialogViewForTesting()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_DESKTOP_MEDIA_PICKER_TITLE_WEB_CONTENTS_ONLY));
}
