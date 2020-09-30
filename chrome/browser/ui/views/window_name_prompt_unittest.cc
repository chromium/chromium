// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

using WindowNamePromptTest = BrowserWithTestWindowTest;

namespace {

views::Widget* ShowPrompt(Browser* browser, gfx::NativeWindow context) {
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      views::BubbleDialogModelHost::kViewClassName);
  chrome::ShowWindowNamePromptForTesting(browser, context);
  return waiter.WaitIfNeededAndGet();
}

views::Textfield* FindTextfield(views::Widget* widget) {
  return static_cast<views::Textfield*>(
      views::test::AnyViewWithClassName(widget, "Textfield"));
}

void SetTextfieldContents(views::Widget* widget, const std::string& text) {
  FindTextfield(widget)->SetText(base::UTF8ToUTF16(text));
}

std::string GetTextfieldContents(views::Widget* widget) {
  return base::UTF16ToUTF8(FindTextfield(widget)->GetText());
}

TEST_F(WindowNamePromptTest, OpensWithInitialName) {
  browser()->SetWindowUserTitle("foobar");

  auto* widget = ShowPrompt(browser(), GetContext());

  EXPECT_EQ(GetTextfieldContents(widget), "foobar");
  views::test::CancelDialog(widget);
}

TEST_F(WindowNamePromptTest, AcceptNonemptySetsName) {
  auto* widget = ShowPrompt(browser(), GetContext());

  EXPECT_EQ(GetTextfieldContents(widget), "");
  SetTextfieldContents(widget, "foo");

  views::test::AcceptDialog(widget);
  EXPECT_EQ(browser()->user_title(), "foo");
}

TEST_F(WindowNamePromptTest, AcceptEmptyClearsName) {
  browser()->SetWindowUserTitle("foo");

  auto* widget = ShowPrompt(browser(), GetContext());

  EXPECT_EQ(GetTextfieldContents(widget), "foo");
  SetTextfieldContents(widget, "");

  views::test::AcceptDialog(widget);
  EXPECT_EQ(browser()->user_title(), "");
}

TEST_F(WindowNamePromptTest, CancelDoesntTouchName) {
  auto* widget = ShowPrompt(browser(), GetContext());
  SetTextfieldContents(widget, "foo");
  views::test::CancelDialog(widget);
  EXPECT_EQ(browser()->user_title(), "");
}

}  // namespace
