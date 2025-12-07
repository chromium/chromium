// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

std::vector<std::unique_ptr<DesktopMediaList>> CreateSourceLists(
    const std::vector<DesktopMediaList::Type>& types) {
  std::vector<std::unique_ptr<DesktopMediaList>> sources;
  for (auto type : types) {
    sources.push_back(std::make_unique<FakeDesktopMediaList>(type));
  }
  return sources;
}

void AddSources(FakeDesktopMediaList* source_list,
                const std::vector<std::u16string>& names) {
  for (size_t i = 0; i < names.size(); ++i) {
    source_list->AddSource(i);
    source_list->SetSourceName(i, names[i]);
  }
}

}  // namespace

class DesktopMediaPickerViewsBrowserTest : public DialogBrowserTest {
 public:
  DesktopMediaPickerViewsBrowserTest() = default;

  DesktopMediaPickerViewsBrowserTest(
      const DesktopMediaPickerViewsBrowserTest&) = delete;
  DesktopMediaPickerViewsBrowserTest& operator=(
      const DesktopMediaPickerViewsBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    picker_ = std::make_unique<DesktopMediaPickerImpl>();
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();

    std::vector<std::unique_ptr<DesktopMediaList>> sources;
    if (override_source_lists_.empty()) {
      sources = CreateDefaultSourceLists();
    } else {
      sources = std::move(override_source_lists_);
    }

    std::vector<FakeDesktopMediaList*> source_lists;
    for (const auto& source : sources) {
      source_lists.push_back(static_cast<FakeDesktopMediaList*>(source.get()));
    }

    DesktopMediaPicker::Params picker_params{request_source_};
    picker_params.web_contents = web_contents;
    picker_params.context = native_window;
    picker_params.app_name = app_name_;
    picker_params.target_name = u"target_name";
    picker_params.request_audio = true;
    picker_->Show(picker_params, std::move(sources),
                  DesktopMediaPicker::DoneCallback());

    if (after_show_callback_) {
      std::move(after_show_callback_).Run(source_lists);
    }
  }

 protected:
  std::vector<std::unique_ptr<DesktopMediaList>> CreateDefaultSourceLists() {
    return CreateSourceLists({DesktopMediaList::Type::kScreen,
                              DesktopMediaList::Type::kWindow,
                              DesktopMediaList::Type::kWebContents});
  }

  std::unique_ptr<DesktopMediaPickerImpl> picker_;

  // If this list isn't filled in, a default list of source lists will be
  // created.
  std::vector<std::unique_ptr<DesktopMediaList>> override_source_lists_;

  DesktopMediaPicker::Params::RequestSource request_source_ =
      DesktopMediaPicker::Params::RequestSource::kUnknown;
  std::u16string app_name_ = u"app_name";

  // This callback is called in ShowUi after the picker dialog has been shown.
  // This both more closely mirrors how this code behaves in production (where
  // the DesktopMediaList is filled asynchronously, so it starts off empty and
  // then becomes filled after the UI is showing) and allows InvokeUi-style
  // tests to update the UI state after showing it.
  base::OnceCallback<void(const std::vector<FakeDesktopMediaList*>&)>
      after_show_callback_;
};

// Invokes a dialog that allows the user to select what view of their desktop
// they would like to share.
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest, InvokeUi_default) {
  after_show_callback_ =
      base::BindOnce([](const std::vector<FakeDesktopMediaList*>& sources) {
        AddSources(sources[0], {u"0"});
        AddSources(sources[1],
                   {u"Warty Warthog", u"Hoary Hedgehog", u"Breezy Badger"});
        AddSources(sources[2], {u"Dapper Drake", u"Edgy Eft", u"Feisty Fawn"});
      });
  ShowAndVerifyUi();
}

// Show the picker UI with only one source type: TYPE_WEB_CONTENTS, aka the
// tab picker.
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest, InvokeUi_tabs) {
  after_show_callback_ =
      base::BindOnce([](const std::vector<FakeDesktopMediaList*>& sources) {
        AddSources(sources[0], {u"Dapper Drake", u"Edgy Eft", u"Feisty Fawn"});
      });
  override_source_lists_ =
      CreateSourceLists({DesktopMediaList::Type::kWebContents});
  ShowAndVerifyUi();
}

class DesktopMediaPickerViewsLongTitleBrowserTest
    : public DesktopMediaPickerViewsBrowserTest {
 public:
  DesktopMediaPickerViewsLongTitleBrowserTest() {
    feature_list_.InitAndEnableFeature(kDesktopMediaPickerMultiLineTitle);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Show the getDisplayMedia picker UI with a very long title that should wrap.
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsLongTitleBrowserTest,
                       InvokeUi_LongTitle) {
  request_source_ = DesktopMediaPicker::Params::RequestSource::kGetDisplayMedia;
  app_name_ =
      u"a.site.with.a.super.long.name.that.needs.to.be.displayed.over.multiple."
      u"lines.in.the.getdisplaymedia.picker.dialog.this.is.a.very.long.string."
      u"to.test.the.ui.and.make.sure.it.does.not.break.with.long.inputs.and.we."
      u"will.add.some.more.text.to.reach.the.limit.";
  after_show_callback_ =
      base::BindOnce([](const std::vector<FakeDesktopMediaList*>& sources) {
        AddSources(sources[0], {u"Dapper Drake", u"Edgy Eft", u"Feisty Fawn"});
        AddSources(sources[1],
                   {u"Warty Warthog", u"Hoary Hedgehog", u"Breezy Badger"});
        AddSources(sources[2], {u""});
      });
  override_source_lists_ = CreateSourceLists(
      {DesktopMediaList::Type::kWebContents, DesktopMediaList::Type::kWindow,
       DesktopMediaList::Type::kScreen});

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
      ui::mojom::DialogButton::kOk));
}

// Validate that the dialog title changes to match the source type when there's
// only one source type present.
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest,
                       SingleSourceTypeChangesTitle) {
  override_source_lists_ =
      CreateSourceLists({DesktopMediaList::Type::kWebContents});
  ShowUi(std::string());

  EXPECT_EQ(picker_->GetDialogViewForTesting()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_DESKTOP_MEDIA_PICKER_TITLE_WEB_CONTENTS_ONLY));
}

// Validate that the scroll view min height is correct
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest,
                       CorrectScrollViewMinHeight) {
  override_source_lists_ = CreateSourceLists({DesktopMediaList::Type::kScreen});
  ShowUi(std::string());

  auto* scroll_view = picker_->GetDialogViewForTesting()->GetViewByID(
      VIEW_ID_MEDIA_PICKER_SCREEN_SCROLL_VIEW);
  int expected_height = GetGenericScreenStyle().item_size.height() +
                        GetGenericScreenStyle().label_rect.height();
  EXPECT_EQ(scroll_view->bounds().height(), expected_height);
}
