// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/prompt_behavior.h"

#include <memory>
#include <unordered_map>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace prompt_behavior {
extern const char kAccept[] = "accept";
extern const char kAcceptAndNotify[] = "accept and notify";
extern const char kDismiss[] = "dismiss";
extern const char kDismissAndNotify[] = "dismiss and notify";
extern const char kIgnore[] = "ignore";
}  // namespace prompt_behavior

namespace dialog_types {
extern const char kAlert[] = "alert";
extern const char kBeforeUnload[] = "beforeUnload";
extern const char kConfirm[] = "confirm";
extern const char kPrompt[] = "prompt";
}  // namespace dialog_types

PromptBehavior::PromptBehavior() : PromptBehavior(true) {}

PromptBehavior::PromptBehavior(bool w3c_compliant) {
  original_capability_value =
      w3c_compliant ? base::Value(prompt_behavior::kDismissAndNotify)
                    : base::Value(prompt_behavior::kIgnore);

  // Default for W3C compliant mode is 'dismiss and notify'. For legacy mode is
  // 'ignore'.
  PromptHandlerConfiguration default_ = {PromptHandlerType::kDismiss, true};
  if (!w3c_compliant) {
    default_ = {PromptHandlerType::kIgnore, true};
  }

  alert = default_;
  confirm = default_;
  prompt = default_;

  // Before unload is a special case. Default is 'accept' unless a dict with
  // explicit `beforeUnload` or `default` value is provided in capability
  // `unhandledPromptBehavior`.
  before_unload = {PromptHandlerType::kAccept, false};
}

namespace {
Status ParsePromptHandlerConfiguration(const std::string& type_str,
                                       bool w3c_compliant,
                                       PromptHandlerConfiguration& type) {
  static const std::unordered_map<std::string, PromptHandlerConfiguration>
      prompt_behavior_to_configuration_map = {
          {prompt_behavior::kDismiss, {PromptHandlerType::kDismiss, false}},
          {prompt_behavior::kDismissAndNotify,
           {PromptHandlerType::kDismiss, true}},
          {prompt_behavior::kAccept, {PromptHandlerType::kAccept, false}},
          {prompt_behavior::kAcceptAndNotify,
           {PromptHandlerType::kAccept, true}},
          {prompt_behavior::kIgnore, {PromptHandlerType::kIgnore, true}},
      };

  if (!prompt_behavior_to_configuration_map.contains(type_str)) {
    return Status(kInvalidArgument,
                  "Unexpected value " + type_str + " in capabilities");
  }

  type = prompt_behavior_to_configuration_map.at(type_str);
  if (!w3c_compliant) {
    // For backward compatibility, in legacy non-w3c compliant mode we always
    // notify.
    type.notify = true;
  }
  return Status{kOk};
}

Status ParsePromptHandlerConfiguration(const base::Value* type_value,
                                       bool w3c_compliant,
                                       PromptHandlerConfiguration& type) {
  if (!type_value || !type_value->is_string()) {
    return Status(kInvalidArgument, "Unexpected type value in capabilities");
  }

  return ParsePromptHandlerConfiguration(type_value->GetString(), w3c_compliant,
                                         type);
}

Status GetDefaultHandlerConfiguration(
    const std::string& dialog_type,
    const base::Value::Dict& prompt_behavior_dict,
    bool w3c_compliant,
    PromptHandlerConfiguration& result) {
  if (prompt_behavior_dict.contains("default")) {
    // Set default value to the one specified in the capability dict.
    Status status = ParsePromptHandlerConfiguration(
        prompt_behavior_dict.Find("default"), w3c_compliant, result);
    return status;
  }

  if (dialog_type == dialog_types::kBeforeUnload) {
    // Before unload is a special case. Default is 'accept' unless a dict with
    // explicit `beforeUnload` or `default` value is provided in capability
    // `unhandledPromptBehavior`.
    result = {PromptHandlerType::kAccept, false};
    return Status{kOk};
  }
  // For all other handlers the default is 'dismiss and notify' or `ignore`
  // depending on w3c compliance mode.
  result = {
      w3c_compliant ? PromptHandlerType::kDismiss : PromptHandlerType::kIgnore,
      true};
  return Status{kOk};
}

Status FillInHandlerConfiguration(const std::string& dialog_type,
                                  const base::Value::Dict& prompt_behavior_dict,
                                  bool w3c_compliant,
                                  PromptHandlerConfiguration& result) {
  if (prompt_behavior_dict.contains(dialog_type)) {
    Status status = ParsePromptHandlerConfiguration(
        prompt_behavior_dict.Find(dialog_type), w3c_compliant, result);
    return status;
  }

  // Set default value to the one specified in the capability dict.
  Status status = GetDefaultHandlerConfiguration(
      dialog_type, prompt_behavior_dict, w3c_compliant, result);

  return status;
}
}  // namespace

Status PromptBehavior::Create(bool w3c_compliant,
                              const std::string& prompt_behavior_str,
                              PromptBehavior& result) {
  result = PromptBehavior(w3c_compliant);
  PromptHandlerConfiguration prompt_handler_configuration;
  Status status = ParsePromptHandlerConfiguration(
      prompt_behavior_str, w3c_compliant, prompt_handler_configuration);
  if (status.IsError()) {
    return status;
  }

  result.alert = prompt_handler_configuration;
  result.confirm = prompt_handler_configuration;
  result.prompt = prompt_handler_configuration;

  // Before unload is a special case. Default is 'accept' unless a dict with
  // explicit `beforeUnload` or `default` value is provided in capability
  // `unhandledPromptBehavior`.
  result.before_unload = {PromptHandlerType::kAccept, false};

  return Status{kOk};
}

Status PromptBehavior::Create(bool w3c_compliant, PromptBehavior& result) {
  result = PromptBehavior(w3c_compliant);
  return Status{kOk};
}

Status PromptBehavior::Create(bool w3c_compliant,
                              const base::Value::Dict& prompt_behavior_dict,
                              PromptBehavior& result) {
  result = PromptBehavior(w3c_compliant);

  // Maps dialog type name to reference to the corresponding handler.
  std::vector<std::tuple<std::string, PromptHandlerConfiguration&>>
      prompt_types_to_fill = {
          {dialog_types::kAlert, result.alert},
          {dialog_types::kBeforeUnload, result.before_unload},
          {dialog_types::kConfirm, result.confirm},
          {dialog_types::kPrompt, result.prompt},
      };
  for (auto& to_fill : prompt_types_to_fill) {
    auto [dialog_type, configuration_destination_reference] = to_fill;
    Status status = FillInHandlerConfiguration(
        dialog_type, prompt_behavior_dict, w3c_compliant,
        configuration_destination_reference);
    if (status.IsError()) {
      return status;
    }
  }
  return Status{kOk};
}

Status PromptBehavior::Create(bool w3c_compliant,
                              const base::Value& prompt_behavior_value,
                              PromptBehavior& result) {
  if (prompt_behavior_value.is_string()) {
    Status status =
        Create(w3c_compliant, prompt_behavior_value.GetString(), result);
    if (status.IsError()) {
      return status;
    }
  } else if (prompt_behavior_value.is_dict()) {
    Status status =
        Create(w3c_compliant, prompt_behavior_value.GetDict(), result);
    if (status.IsError()) {
      return status;
    }
  } else {
    return Status(kInvalidArgument,
                  "Capability `unhandledPromptBehavior` should be a string or "
                  "a dictionary.");
  }

  result.original_capability_value = prompt_behavior_value.Clone();
  return Status{kOk};
}

Status PromptBehavior::GetConfiguration(
    const std::string& dialog_type,
    PromptHandlerConfiguration& handler_configuration) {
  if (dialog_type == dialog_types::kAlert) {
    handler_configuration = alert;
  } else if (dialog_type == dialog_types::kBeforeUnload) {
    handler_configuration = before_unload;
  } else if (dialog_type == dialog_types::kConfirm) {
    handler_configuration = confirm;
  } else if (dialog_type == dialog_types::kPrompt) {
    handler_configuration = prompt;
  } else {
    return Status(kInvalidArgument, "Unexpected dialog type " + dialog_type);
  }

  return Status{kOk};
}

base::Value PromptBehavior::MapperOptionsView() {
  // Maps prompt behavior type to string.
  static const std::unordered_map<PromptHandlerType, std::string>
      prompt_handler_type_to_string = {
          {PromptHandlerType::kAccept, prompt_behavior::kAccept},
          {PromptHandlerType::kDismiss, prompt_behavior::kDismiss},
          {PromptHandlerType::kIgnore, prompt_behavior::kIgnore},
      };

  base::Value::Dict result_dict;
  // List of fields to be filled in the result dictionary.
  std::vector<std::pair<std::string, PromptHandlerType>> dialog_to_handler = {
      {dialog_types::kAlert, alert.type},
      {dialog_types::kBeforeUnload, before_unload.type},
      {dialog_types::kConfirm, confirm.type},
      {dialog_types::kPrompt, prompt.type},
  };

  for (std::pair<std::string, PromptHandlerType>& item : dialog_to_handler) {
    auto [dialog_type, handler_type] = item;
    result_dict.Set(dialog_type,
                    prompt_handler_type_to_string.at(handler_type));
  }

  return base::Value(std::move(result_dict));
}

base::Value PromptBehavior::CapabilityView() {
  return original_capability_value.Clone();
}
