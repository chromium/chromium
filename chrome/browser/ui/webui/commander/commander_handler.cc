// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commander/commander_handler.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/commander/commander_view_model.h"

const char CommanderHandler::Delegate::kKey[] =
    "CommanderHandler::Delegate::kKey";
CommanderHandler::CommanderHandler() = default;
CommanderHandler::~CommanderHandler() = default;

void CommanderHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "textChanged", base::BindRepeating(&CommanderHandler::HandleTextChanged,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "optionSelected",
      base::BindRepeating(&CommanderHandler::HandleOptionSelected,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dismiss", base::BindRepeating(&CommanderHandler::HandleDismiss,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "heightChanged",
      base::BindRepeating(&CommanderHandler::HandleHeightChanged,
                          base::Unretained(this)));
}

void CommanderHandler::OnJavascriptDisallowed() {
  if (delegate_)
    delegate_->OnHandlerEnabled(false);
}

void CommanderHandler::OnJavascriptAllowed() {
  if (delegate_)
    delegate_->OnHandlerEnabled(true);
}

void CommanderHandler::HandleTextChanged(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1u, args->GetSize());
  std::string text = args->GetList()[0].GetString();
  if (delegate_)
    delegate_->OnTextChanged(base::UTF8ToUTF16(text));
}

void CommanderHandler::HandleOptionSelected(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2u, args->GetSize());
  int index = args->GetList()[0].GetInt();
  int result_set_id = args->GetList()[1].GetInt();
  if (delegate_)
    delegate_->OnOptionSelected(index, result_set_id);
}

void CommanderHandler::HandleDismiss(const base::ListValue* args) {
  if (delegate_)
    delegate_->OnDismiss();
}

void CommanderHandler::HandleHeightChanged(const base::ListValue* args) {
  CHECK_EQ(1u, args->GetSize());
  int new_height = args->GetList()[0].GetInt();
  if (delegate_)
    delegate_->OnHeightChanged(new_height);
}

void CommanderHandler::ViewModelUpdated(
    commander::CommanderViewModel view_model) {
  if (view_model.action ==
      commander::CommanderViewModel::Action::kDisplayResults) {
    base::Value list(base::Value::Type::LIST);
    for (commander::CommandItemViewModel& item : view_model.items) {
      // TODO(lgrey): This is temporary, just so we can display something.
      // We will also need to pass on the result set id, and match ranges for
      // each item.
      list.Append(item.title);
    }
    FireWebUIListener("view-model-updated", list);
  } else {
    DCHECK_EQ(view_model.action,
              commander::CommanderViewModel::Action::kPrompt);
    // TODO(lgrey): Handle kPrompt. kDismiss is handled higher up the stack.
  }
}
