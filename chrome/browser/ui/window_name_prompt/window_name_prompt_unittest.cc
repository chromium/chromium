// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/test/test_dialog_model_host.h"

using WindowNamePromptTest = BrowserWithTestWindowTest;

namespace {

// TODO(pbos): Should some of these functions move into TestDialogModelHost or
// other DialogModel test utilities?
ui::DialogModelTextfield* FindTextfield(ui::DialogModel* dialog_model) {
  for (const auto& field :
       dialog_model->fields(ui::TestDialogModelHost::GetPassKey())) {
    if (field->type(ui::TestDialogModelHost::GetPassKey()) ==
        ui::DialogModelField::kTextfield) {
      return field->AsTextfield(ui::TestDialogModelHost::GetPassKey());
    }
  }
  NOTREACHED();
  return nullptr;
}

void SetTextfieldContents(ui::DialogModel* dialog_model,
                          const std::string& text) {
  FindTextfield(dialog_model)
      ->OnTextChanged(ui::TestDialogModelHost::GetPassKey(),
                      base::UTF8ToUTF16(text));
}

std::string GetTextfieldContents(ui::DialogModel* dialog_model) {
  return base::UTF16ToUTF8(FindTextfield(dialog_model)->text());
}

TEST_F(WindowNamePromptTest, OpensWithInitialName) {
  browser()->SetWindowUserTitle("foobar");

  std::unique_ptr<ui::DialogModel> model =
      chrome::CreateWindowNamePromptDialogModelForTesting(browser());

  EXPECT_EQ(GetTextfieldContents(model.get()), "foobar");
}

TEST_F(WindowNamePromptTest, AcceptNonemptySetsName) {
  std::unique_ptr<ui::DialogModel> model =
      chrome::CreateWindowNamePromptDialogModelForTesting(browser());

  EXPECT_EQ(GetTextfieldContents(model.get()), "");
  SetTextfieldContents(model.get(), "foo");

  // TODO(pbos): Add proper hosting capability to TestDialogModelHost so that
  // this does not need to be inlined here.
  model->OnDialogAcceptAction(ui::TestDialogModelHost::GetPassKey());
  model->OnDialogDestroying(ui::TestDialogModelHost::GetPassKey());
  model.reset();

  EXPECT_EQ(browser()->user_title(), "foo");
}

TEST_F(WindowNamePromptTest, AcceptEmptyClearsName) {
  browser()->SetWindowUserTitle("foo");

  std::unique_ptr<ui::DialogModel> model =
      chrome::CreateWindowNamePromptDialogModelForTesting(browser());

  EXPECT_EQ(GetTextfieldContents(model.get()), "foo");
  SetTextfieldContents(model.get(), "");

  // TODO(pbos): Add proper hosting capability to TestDialogModelHost so that
  // this does not need to be inlined here.
  model->OnDialogAcceptAction(ui::TestDialogModelHost::GetPassKey());
  model->OnDialogDestroying(ui::TestDialogModelHost::GetPassKey());
  model.reset();

  EXPECT_EQ(browser()->user_title(), "");
}

TEST_F(WindowNamePromptTest, CancelDoesntTouchName) {
  std::unique_ptr<ui::DialogModel> model =
      chrome::CreateWindowNamePromptDialogModelForTesting(browser());
  SetTextfieldContents(model.get(), "foo");

  // TODO(pbos): Add proper hosting capability to TestDialogModelHost so that
  // this does not need to be inlined here.
  model->OnDialogCancelAction(ui::TestDialogModelHost::GetPassKey());
  model->OnDialogDestroying(ui::TestDialogModelHost::GetPassKey());
  model.reset();

  EXPECT_EQ(browser()->user_title(), "");
}

}  // namespace
