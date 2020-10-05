// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commander/commander_handler.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/commander/commander_view_model.h"

namespace {
// Message handler keys.
constexpr char kTextChangedMessage[] = "textChanged";
constexpr char kOptionSelectedMessage[] = "optionSelected";
constexpr char kDismissMessage[] = "dismiss";
constexpr char kHeightChangedMessage[] = "heightChanged";
// WebUI event keys.
constexpr char kViewModelUpdatedEvent[] = "view-model-updated";
constexpr char kInitializeEvent[] = "initialize";
// View model dictionary keys
constexpr char kActionKey[] = "action";
constexpr char kResultSetIdKey[] = "result_set_id";
constexpr char kTitleKey[] = "title";
constexpr char kEntityKey[] = "entity";
constexpr char kAnnotationKey[] = "annotation";
constexpr char kMatchedRangesKey[] = "matched_ranges";
constexpr char kOptionsKey[] = "options";
}  // namespace

CommanderHandler::CommanderHandler() = default;
CommanderHandler::~CommanderHandler() = default;

void CommanderHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kTextChangedMessage,
      base::BindRepeating(&CommanderHandler::HandleTextChanged,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kOptionSelectedMessage,
      base::BindRepeating(&CommanderHandler::HandleOptionSelected,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kDismissMessage, base::BindRepeating(&CommanderHandler::HandleDismiss,
                                           base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kHeightChangedMessage,
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
    base::DictionaryValue vm;
    vm.SetIntKey(kActionKey, view_model.action);
    vm.SetIntKey(kResultSetIdKey, view_model.result_set_id);
    base::ListValue option_list;
    for (commander::CommandItemViewModel& item : view_model.items) {
      base::DictionaryValue option;
      option.SetStringKey(kTitleKey, item.title);
      option.SetIntKey(kEntityKey, item.entity_type);
      if (!item.annotation.empty())
        option.SetStringKey(kAnnotationKey, item.annotation);
      base::ListValue ranges;
      for (const gfx::Range& range : item.matched_ranges) {
        base::ListValue range_value;
        range_value.Append(static_cast<int>(range.start()));
        range_value.Append(static_cast<int>(range.end()));
        ranges.Append(std::move(range_value));
      }
      option.SetKey(kMatchedRangesKey, std::move(ranges));
      option_list.Append(std::move(option));
    }
    vm.SetKey(kOptionsKey, std::move(option_list));
    FireWebUIListener(kViewModelUpdatedEvent, vm);
  } else {
    DCHECK_EQ(view_model.action,
              commander::CommanderViewModel::Action::kPrompt);
    // TODO(lgrey): Handle kPrompt. kDismiss is handled higher up the stack.
  }
}

void CommanderHandler::PrepareToShow(Delegate* delegate) {
  delegate_ = delegate;
  if (IsJavascriptAllowed())
    FireWebUIListener(kInitializeEvent);
}
