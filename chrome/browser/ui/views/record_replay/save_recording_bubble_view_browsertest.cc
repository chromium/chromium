// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/record_replay/save_recording_bubble_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace record_replay {

class FakeSaveRecordingBubbleController : public SaveRecordingBubbleController {
 public:
  explicit FakeSaveRecordingBubbleController(const std::string& url)
      : url_(url) {}
  ~FakeSaveRecordingBubbleController() override = default;

  void OnSave(std::u16string_view name) override { name_ = name; }
  void OnCancel() override { cancelled_ = true; }
  void OnBubbleClosed() override { closed_ = true; }
  std::string_view GetUrl() const override { return url_; }

  std::u16string_view name() const { return name_; }
  bool cancelled() const { return cancelled_; }
  bool closed() const { return closed_; }

 private:
  std::string url_;
  std::u16string name_;
  bool cancelled_ = false;
  bool closed_ = false;
};

class SaveRecordingBubbleViewTest : public DialogBrowserTest {
 public:
  SaveRecordingBubbleViewTest() = default;
  ~SaveRecordingBubbleViewTest() override = default;

  void ShowUi(const std::string& name) override {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::BubbleAnchor anchor(browser_view->toolbar());

    auto controller = std::make_unique<FakeSaveRecordingBubbleController>(
        "https://example.com");

    widget_ = SaveRecordingBubbleView::Show(
        anchor, browser()->tab_strip_model()->GetActiveWebContents(),
        std::move(controller));
  }

  SaveRecordingBubbleView* GetBubbleView() {
    if (!widget_) {
      return nullptr;
    }
    return static_cast<SaveRecordingBubbleView*>(widget_->widget_delegate());
  }

  void DismissUi() override {
    widget_ = nullptr;
  }

 protected:
  raw_ptr<views::Widget> widget_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SaveRecordingBubbleViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SaveRecordingBubbleViewTest, PrefilledNameIsNotEmpty) {
  ShowUi("");
  SaveRecordingBubbleView* bubble = GetBubbleView();
  ASSERT_NE(bubble, nullptr);

  views::Textfield* textfield = bubble->name_textfield_for_testing();
  ASSERT_NE(textfield, nullptr);

  // The default name should be generated from the URL (example.com) and date.
  // So it should not be empty.
  EXPECT_FALSE(textfield->GetText().empty());

  // The OK button should be enabled because the text is not empty.
  EXPECT_TRUE(bubble->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  DismissUi();
}

IN_PROC_BROWSER_TEST_F(SaveRecordingBubbleViewTest,
                       EmptyNameDisablesSaveButton) {
  ShowUi("");
  SaveRecordingBubbleView* bubble = GetBubbleView();
  ASSERT_NE(bubble, nullptr);

  views::Textfield* textfield = bubble->name_textfield_for_testing();
  ASSERT_NE(textfield, nullptr);

  // Clear the text field.
  textfield->SetText(u"");
  bubble->ContentsChanged(textfield, u"");

  // The OK button should be disabled now.
  EXPECT_FALSE(bubble->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  DismissUi();
}

IN_PROC_BROWSER_TEST_F(SaveRecordingBubbleViewTest, TypingEnablesSaveButton) {
  ShowUi("");
  SaveRecordingBubbleView* bubble = GetBubbleView();
  ASSERT_NE(bubble, nullptr);

  views::Textfield* textfield = bubble->name_textfield_for_testing();
  ASSERT_NE(textfield, nullptr);

  // Clear the text field.
  textfield->SetText(u"");
  bubble->ContentsChanged(textfield, u"");
  EXPECT_FALSE(bubble->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Type something.
  textfield->SetText(u"My Test");
  bubble->ContentsChanged(textfield, u"My Test");

  // The OK button should be enabled.
  EXPECT_TRUE(bubble->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  DismissUi();
}

}  // namespace record_replay
