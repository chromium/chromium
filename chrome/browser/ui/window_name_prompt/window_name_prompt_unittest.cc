// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/test/test_dialog_model_host.h"

using WindowNamePromptTest = BrowserWithTestWindowTest;

namespace {

void SetTextfieldContents(ui::TestDialogModelHost* host,
                          const std::string& text) {
  host->SetSingleTextfieldText(base::UTF8ToUTF16(text));
}

std::string GetTextfieldContents(ui::TestDialogModelHost* host) {
  return base::UTF16ToUTF8(host->FindSingleTextfield()->text());
}

TEST_F(WindowNamePromptTest, OpensWithInitialName) {
  browser()->SetWindowUserTitle("foobar");

  auto host = std::make_unique<ui::TestDialogModelHost>(
      chrome::CreateWindowNamePromptDialogModelForTesting(browser()));

  EXPECT_EQ(GetTextfieldContents(host.get()), "foobar");
}

TEST_F(WindowNamePromptTest, AcceptNonemptySetsName) {
  auto host = std::make_unique<ui::TestDialogModelHost>(
      chrome::CreateWindowNamePromptDialogModelForTesting(browser()));

  EXPECT_EQ(GetTextfieldContents(host.get()), "");
  SetTextfieldContents(host.get(), "foo");

  ui::TestDialogModelHost::Accept(std::move(host));

  EXPECT_EQ(browser()->user_title(), "foo");
}

TEST_F(WindowNamePromptTest, AcceptEmptyClearsName) {
  browser()->SetWindowUserTitle("foo");

  auto host = std::make_unique<ui::TestDialogModelHost>(
      chrome::CreateWindowNamePromptDialogModelForTesting(browser()));

  EXPECT_EQ(GetTextfieldContents(host.get()), "foo");
  SetTextfieldContents(host.get(), "");

  ui::TestDialogModelHost::Accept(std::move(host));

  EXPECT_EQ(browser()->user_title(), "");
}

TEST_F(WindowNamePromptTest, CancelDoesntTouchName) {
  auto host = std::make_unique<ui::TestDialogModelHost>(
      chrome::CreateWindowNamePromptDialogModelForTesting(browser()));
  SetTextfieldContents(host.get(), "foo");

  ui::TestDialogModelHost::Cancel(std::move(host));

  EXPECT_EQ(browser()->user_title(), "");
}

}  // namespace
