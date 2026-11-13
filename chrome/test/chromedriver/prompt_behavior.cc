// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/prompt_behavior.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {
constexpr char kDefaultPromptKey[] = "default";
constexpr char kFilePromptKey[] = "file";
}  // namespace

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
  static constexpr auto kPromptBehaviorToConfigurationMap =
      base::MakeFixedFlatMap<std::string_view, PromptHandlerConfiguration>({
          {prompt_behavior::kDismiss, {PromptHandlerType::kDismiss, false}},
          {prompt_behavior::kDismissAndNotify,
           {PromptHandlerType::kDismiss, true}},
          {prompt_behavior::kAccept, {PromptHandlerType::kAccept, false}},
          {prompt_behavior::kAcceptAndNotify,
           {PromptHandlerType::kAccept, true}},
          {prompt_behavior::kIgnore, {PromptHandlerType::kIgnore, true}},
      });

  auto it = kPromptBehaviorToConfigurationMap.find(type_str);
  if (it == kPromptBehaviorToConfigurationMap.end()) {
    return Status(kInvalidArgument,
                  "Unexpected value " + type_str + " in capabilities");
  }

  type = it->second;
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
    std::string_view dialog_type,
    const base::DictValue& prompt_behavior_dict,
    bool w3c_compliant,
    PromptHandlerConfiguration& result) {
  if (prompt_behavior_dict.contains(kDefaultPromptKey)) {
    // Set default value to the one specified in the capability dict.
    Status status = ParsePromptHandlerConfiguration(
        prompt_behavior_dict.Find(kDefaultPromptKey), w3c_compliant, result);
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

Status FillInHandlerConfiguration(std::string_view dialog_type,
                                  const base::DictValue& prompt_behavior_dict,
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
                              const base::DictValue& prompt_behavior_dict,
                              PromptBehavior& result) {
  result = PromptBehavior(w3c_compliant);

  // Every key must be a known prompt handler key. Unknown keys are rejected so
  // that invalid handler configurations are not silently ignored.
  static constexpr std::array<std::string_view, 6> kValidPromptKeys = {
      dialog_types::kAlert,   dialog_types::kBeforeUnload,
      dialog_types::kConfirm, kDefaultPromptKey,
      kFilePromptKey,         dialog_types::kPrompt};
  for (auto it : prompt_behavior_dict) {
    if (!std::ranges::contains(kValidPromptKeys, it.first)) {
      return Status(kInvalidArgument,
                    "Unexpected key " + it.first +
                        " in capability `unhandledPromptBehavior`");
    }

    PromptHandlerConfiguration configuration;
    Status status = ParsePromptHandlerConfiguration(&it.second, w3c_compliant,
                                                    configuration);
    if (status.IsError()) {
      return status;
    }
  }

  // The BiDi mapper applies the file prompt handler. ChromeDriver only stores
  // configurations for JavaScript dialogs.
  // Maps dialog type name to the corresponding handler member.
  static constexpr auto kPromptTypesToFill =
      std::to_array<std::pair<std::string_view,
                              PromptHandlerConfiguration PromptBehavior::*>>({
          {dialog_types::kAlert, &PromptBehavior::alert},
          {dialog_types::kBeforeUnload, &PromptBehavior::before_unload},
          {dialog_types::kConfirm, &PromptBehavior::confirm},
          {dialog_types::kPrompt, &PromptBehavior::prompt},
      });
  for (const auto& [dialog_type, handler] : kPromptTypesToFill) {
    Status status = FillInHandlerConfiguration(
        dialog_type, prompt_behavior_dict, w3c_compliant, result.*handler);
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
  static constexpr auto kPromptHandlerTypeToString =
      base::MakeFixedFlatMap<PromptHandlerType, std::string_view>({
          {PromptHandlerType::kAccept, prompt_behavior::kAccept},
          {PromptHandlerType::kDismiss, prompt_behavior::kDismiss},
          {PromptHandlerType::kIgnore, prompt_behavior::kIgnore},
      });

  base::DictValue result_dict;
  // List of fields to be filled in the result dictionary.
  static constexpr auto kDialogToHandler =
      std::to_array<std::pair<std::string_view,
                              PromptHandlerConfiguration PromptBehavior::*>>({
          {dialog_types::kAlert, &PromptBehavior::alert},
          {dialog_types::kBeforeUnload, &PromptBehavior::before_unload},
          {dialog_types::kConfirm, &PromptBehavior::confirm},
          {dialog_types::kPrompt, &PromptBehavior::prompt},
      });

  for (const auto& [dialog_type, handler] : kDialogToHandler) {
    result_dict.Set(dialog_type,
                    kPromptHandlerTypeToString.at((this->*handler).type));
  }

  return base::Value(std::move(result_dict));
}

base::Value PromptBehavior::CapabilityView() {
  return original_capability_value.Clone();
}
