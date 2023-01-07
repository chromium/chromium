// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commander/commander_handler.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/commander/commander_view_model.h"

namespace {
// Message handler keys.
constexpr char kTextChangedMessage[] = "textChanged";
constexpr char kOptionSelectedMessage[] = "optionSelected";
constexpr char kDismissMessage[] = "dismiss";
constexpr char kHeightChangedMessage[] = "heightChanged";
constexpr char kCompositeCommandCancelledMessage[] =
    "compositeCommandCancelled";
// WebUI event keys.
constexpr char kViewModelUpdatedEvent[] = "view-model-updated";
constexpr char kInitializeEvent[] = "initialize";
// View model dictionary keys
constexpr char kActionKey[] = "action";
constexpr char kResultSetIdKey[] = "resultSetId";
constexpr char kTitleKey[] = "title";
constexpr char kEntityKey[] = "entity";
constexpr char kAnnotationKey[] = "annotation";
constexpr char kMatchedRangesKey[] = "matchedRanges";
constexpr char kOptionsKey[] = "options";
constexpr char kPromptTextKey[] = "promptText";
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
      kCompositeCommandCancelledMessage,
      base::BindRepeating(&CommanderHandler::HandleCompositeCommandCancelled,
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

void CommanderHandler::HandleTextChanged(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1u, args.size());
  std::string text = args[0].GetString();
  if (delegate_)
    delegate_->OnTextChanged(base::UTF8ToUTF16(text));
}

void CommanderHandler::HandleOptionSelected(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2u, args.size());
  int index = args[0].GetInt();
  int result_set_id = args[1].GetInt();
  if (delegate_)
    delegate_->OnOptionSelected(index, result_set_id);
}

void CommanderHandler::HandleCompositeCommandCancelled(
    const base::Value::List& args) {
  if (!delegate_)
    return;
  AllowJavascript();
  delegate_->OnCompositeCommandCancelled();
}

void CommanderHandler::HandleDismiss(const base::Value::List& args) {
  if (delegate_)
    delegate_->OnDismiss();
}

void CommanderHandler::HandleHeightChanged(const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  int new_height = args[0].GetInt();
  if (delegate_)
    delegate_->OnHeightChanged(new_height);
}

void CommanderHandler::ViewModelUpdated(
    commander::CommanderViewModel view_model) {
  base::Value::Dict vm;
  vm.Set(kActionKey, view_model.action);
  vm.Set(kResultSetIdKey, view_model.result_set_id);
  if (view_model.action ==
      commander::CommanderViewModel::Action::kDisplayResults) {
    base::Value::List option_list;
    for (commander::CommandItemViewModel& item : view_model.items) {
      base::Value::Dict option;
      option.Set(kTitleKey, item.title);
      option.Set(kEntityKey, item.entity_type);
      if (!item.annotation.empty())
        option.Set(kAnnotationKey, item.annotation);
      base::Value::List ranges;
      for (const gfx::Range& range : item.matched_ranges) {
        base::Value::List range_value;
        range_value.Append(static_cast<int>(range.start()));
        range_value.Append(static_cast<int>(range.end()));
        ranges.Append(std::move(range_value));
      }
      option.Set(kMatchedRangesKey, std::move(ranges));
      option_list.Append(std::move(option));
    }
    vm.Set(kOptionsKey, std::move(option_list));
  } else {
    // kDismiss is handled higher in the stack.
    DCHECK_EQ(view_model.action,
              commander::CommanderViewModel::Action::kPrompt);
    vm.Set(kPromptTextKey, view_model.prompt_text);
  }
  FireWebUIListener(kViewModelUpdatedEvent, vm);
}

void CommanderHandler::PrepareToShow(Delegate* delegate) {
  delegate_ = delegate;
  if (IsJavascriptAllowed())
    FireWebUIListener(kInitializeEvent);
}
