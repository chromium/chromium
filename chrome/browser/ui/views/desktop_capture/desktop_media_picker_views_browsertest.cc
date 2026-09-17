// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/desktop_capture/share_this_tab_dialog_views.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/md_text_button.h"
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
    auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();
    gfx::NativeWindow native_window = browser()->GetWindow()->GetNativeWindow();

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
  set_baseline("7638461");
  after_show_callback_ =
      base::BindOnce([](const std::vector<FakeDesktopMediaList*>& sources) {
        AddSources(sources[0], {u"Dapper Drake", u"Edgy Eft", u"Feisty Fawn"});
      });
  override_source_lists_ =
      CreateSourceLists({DesktopMediaList::Type::kWebContents});
  ShowAndVerifyUi();
}

// Show the getDisplayMedia picker UI with a very long title that should wrap.
// TODO(crbug.com/491087314): Fix flaky test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_LongTitle DISABLED_InvokeUi_LongTitle
#else
#define MAYBE_InvokeUi_LongTitle InvokeUi_LongTitle
#endif
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest,
                       MAYBE_InvokeUi_LongTitle) {
  set_baseline("7638461");
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

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
class ShareThisTabDialogViewsProtectionBrowserTest
    : public InProcessBrowserTest {
 public:
  ShareThisTabDialogViewsProtectionBrowserTest() {
    feature_list_.InitAndEnableFeature(
        enterprise_data_protection::kEnableTabSharingProtection);
  }

  ~ShareThisTabDialogViewsProtectionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/title1.html")));
  }

  void TearDownOnMainThread() override {
    picker_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ShareThisTabDialogView* dialog() {
    return picker_ ? picker_->GetDialogViewForTesting() : nullptr;
  }

  ShareThisTabSourceView* source_view() {
    return dialog() ? dialog()->GetSourceViewForTesting() : nullptr;
  }

  void SetScreenshotBlocked(bool blocked) {
    if (blocked) {
      data_controls::SetDataControls(browser()->GetProfile()->GetPrefs(), {R"({
            "name": "block",
            "rule_id": "1234",
            "sources": {"urls": ["*"]},
            "restrictions": [{"class": "SCREENSHOT", "level": "BLOCK"}]
          })"});
    } else {
      data_controls::SetDataControls(browser()->GetProfile()->GetPrefs(), {});
    }
    GURL url = embedded_test_server()->GetURL(
        base::StringPrintf("/title1.html#%d", ++nav_counter_));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  void ShowPicker() {
    ASSERT_TRUE(web_contents());
    DesktopMediaPicker::Params params{
        DesktopMediaPicker::Params::RequestSource::kGetDisplayMedia};
    params.web_contents = web_contents();
    params.context = browser()->GetWindow()->GetNativeWindow();
    params.app_name = u"Test App";
    params.target_name = u"Target";
    params.request_audio = true;

    done_loop_.emplace();
    picker_ = std::make_unique<ShareThisTabMediaPicker>();
    picker_->Show(params, {},
                  base::BindOnce(
                      [](bool* done, base::RepeatingClosure quit_closure,
                         DesktopMediaPicker::DoneCallbackArgumentType result) {
                        *done = true;
                        quit_closure.Run();
                      },
                      &picker_done_, done_loop_->QuitClosure()));
    ASSERT_TRUE(dialog());
    ASSERT_TRUE(source_view());
  }

 protected:
  int nav_counter_ = 0;
  bool picker_done_ = false;
  std::optional<base::RunLoop> done_loop_;
  std::unique_ptr<ShareThisTabMediaPicker> picker_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ShareThisTabDialogViewsProtectionBrowserTest,
                       BlockedTabSuppressesPreviewAndDisablesOkButton) {
  SetScreenshotBlocked(true);
  ShowPicker();

  // Before activation, OK button is disabled and blocked tooltip is present.
  EXPECT_FALSE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(
      source_view()->GetFaviconViewForTesting()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_BLOCKED_TITLE));

  dialog()->ActivateForTesting();

  // After activation, OK button remains disabled.
  EXPECT_FALSE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Blocked label is displayed in place of the live thumbnail.
  EXPECT_TRUE(source_view()->GetBlockedLabelForTesting()->GetVisible());
  EXPECT_EQ(
      source_view()->GetBlockedLabelForTesting()->GetText(),
      l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_BLOCKED_PREVIEW));
  EXPECT_FALSE(source_view()->GetImageViewForTesting()->GetVisible());
  EXPECT_FALSE(source_view()->IsRefreshingForTesting());

  // Attempting to accept the dialog fails.
  EXPECT_FALSE(dialog()->Accept());
  EXPECT_FALSE(picker_done_);

  // Dynamically unblock the tab via DataProtectionNavigationController
  // callback.
  SetScreenshotBlocked(false);

  // OK button should now be enabled and preview restored.
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_FALSE(source_view()->GetBlockedLabelForTesting()->GetVisible());
  EXPECT_TRUE(source_view()->GetImageViewForTesting()->GetVisible());
  EXPECT_TRUE(source_view()->IsRefreshingForTesting());
  EXPECT_TRUE(
      source_view()->GetFaviconViewForTesting()->GetTooltipText().empty());

  // Accepting the dialog should now succeed.
  EXPECT_TRUE(dialog()->Accept());
  done_loop_->Run();
  EXPECT_TRUE(picker_done_);
}

IN_PROC_BROWSER_TEST_F(ShareThisTabDialogViewsProtectionBrowserTest,
                       AllowedTabBecomesBlockedDynamically) {
  ShowPicker();

  dialog()->ActivateForTesting();

  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_FALSE(source_view()->GetBlockedLabelForTesting()->GetVisible());
  EXPECT_TRUE(source_view()->GetImageViewForTesting()->GetVisible());
  EXPECT_TRUE(source_view()->IsRefreshingForTesting());

  // Dynamically block the tab via DataProtectionNavigationController callback.
  SetScreenshotBlocked(true);

  EXPECT_FALSE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(source_view()->GetBlockedLabelForTesting()->GetVisible());
  EXPECT_FALSE(source_view()->GetImageViewForTesting()->GetVisible());
  EXPECT_FALSE(source_view()->IsRefreshingForTesting());
  EXPECT_EQ(
      source_view()->GetFaviconViewForTesting()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_BLOCKED_TITLE));

  EXPECT_FALSE(dialog()->Accept());
  EXPECT_FALSE(picker_done_);

  dialog()->CancelDialog();
  done_loop_->Run();
  EXPECT_TRUE(picker_done_);
}

IN_PROC_BROWSER_TEST_F(ShareThisTabDialogViewsProtectionBrowserTest,
                       AutoAcceptBlockedWhenTabSharingBlocked) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kThisTabCaptureAutoAccept);

  SetScreenshotBlocked(true);
  ShowPicker();

  dialog()->ActivateForTesting();

  // Auto-accept must be suppressed when blocked.
  EXPECT_FALSE(picker_done_);
  EXPECT_FALSE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(source_view()->GetBlockedLabelForTesting()->GetVisible());
  EXPECT_FALSE(dialog()->Accept());
  EXPECT_FALSE(picker_done_);

  dialog()->CancelDialog();
  done_loop_->Run();
  EXPECT_TRUE(picker_done_);
}
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
