// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/activity_log_policy_util.h"

#include <string_view>

#include "base/strings/string_util.h"

namespace extensions::activity_log_policy_util {

TelemetrySignalType GetTelemetrySignalType(const std::string& api_name,
                                           const base::ListValue& args_unsafe,
                                           DomActionType::Type action_type) {
  // --- CATEGORY 1: CONFIDENTIALITY SIGNALS (Data Theft) ---
  if (api_name == "Document.cookie" || api_name == "HTMLInputElement.value" ||
      api_name == "HTMLTextAreaElement.value") {
    // If the caller provided a specific verb (Renderer), strictly enforce
    // GETTER. If they used the catch-all (MODIFIED), trust the signal.
    if (action_type != DomActionType::MODIFIED &&
        action_type != DomActionType::GETTER) {
      return TelemetrySignalType::kNone;
    }
    return TelemetrySignalType::kDOMAccess;
  }

  // --- CATEGORY 2: INTEGRITY SIGNALS (Injection Defense) ---

  // A. Direct Script Execution APIs
  if (api_name == "scripting.executeScript") {
    return TelemetrySignalType::kScriptInjection;
  }

  // B. DOM-Based Injections
  bool is_set_attr = (api_name == "blinkSetAttribute");
  bool is_add_elem = (api_name == "blinkAddElement");

  if (!is_set_attr && !is_add_elem) {
    return TelemetrySignalType::kNone;
  }

  // Pre-processing: Identify tag and attribute context.
  // Deep inspection requires at least one argument (the tag name).
  if (args_unsafe.empty() || !args_unsafe[0].is_string()) {
    return TelemetrySignalType::kNone;
  }

  // Extract the tag from the first argument.
  const std::string& tag = args_unsafe[0].GetString();
  const size_t args_size = args_unsafe.size();

  // 1. Remote Script Injection (The "Dropper" Check)
  if (base::EqualsCaseInsensitiveASCII(tag, "script")) {
    if (is_add_elem) {
      // blinkAddElement: [tag="script", src]
      return TelemetrySignalType::kScriptInjection;
    }
    if (is_set_attr && args_size >= 4 && args_unsafe[1].is_string() &&
        base::EqualsCaseInsensitiveASCII(args_unsafe[1].GetString(), "src")) {
      // blinkSetAttribute: [tag="script", attr="src", old_val, new_val]
      return TelemetrySignalType::kScriptInjection;
    }
  }

  // 2. Executable Element Creation
  if (is_add_elem && base::EqualsCaseInsensitiveASCII(tag, "iframe")) {
    // blinkAddElement: [tag="iframe", src]
    return TelemetrySignalType::kScriptInjection;
  }

  // 3. Attribute-Based Injection (Unified Logic)
  std::string_view attr_name;
  std::string_view attr_val;

  if (is_set_attr && args_size >= 4) {
    // Explicit attribute change.
    // blinkSetAttribute format: [tag, attr_name, old_val, new_val]
    if (args_unsafe[1].is_string()) {
      attr_name = args_unsafe[1].GetString();
    }
    if (args_unsafe[3].is_string()) {
      attr_val = args_unsafe[3].GetString();
    }
  } else if (is_add_elem && args_size >= 2) {
    // Element creation with attributes in payload.
    // blinkAddElement formats vary by element:
    if (base::EqualsCaseInsensitiveASCII(tag, "a")) {
      // ["a", href]
      attr_name = "href";
      if (args_unsafe[1].is_string()) {
        attr_val = args_unsafe[1].GetString();
      }
    } else if (base::EqualsCaseInsensitiveASCII(tag, "link")) {
      // ["link", rel, href]
      if (args_size >= 3) {
        attr_name = "href";
        if (args_unsafe[2].is_string()) {
          attr_val = args_unsafe[2].GetString();
        }
      }
    } else if (base::EqualsCaseInsensitiveASCII(tag, "form")) {
      // ["form", method, action]
      if (args_size >= 3) {
        attr_name = "action";
        if (args_unsafe[2].is_string()) {
          attr_val = args_unsafe[2].GetString();
        }
      }
    } else if (base::EqualsCaseInsensitiveASCII(tag, "input") ||
               base::EqualsCaseInsensitiveASCII(tag, "button")) {
      // [tag, type, formaction]
      if (args_size >= 3) {
        attr_name = "formaction";
        if (args_unsafe[2].is_string()) {
          attr_val = args_unsafe[2].GetString();
        }
      }
    }
  }

  if (!attr_name.empty()) {
    // a. Form Hijacking (Phishing)
    if (base::EqualsCaseInsensitiveASCII(attr_name, "action") ||
        base::EqualsCaseInsensitiveASCII(attr_name, "formaction")) {
      return TelemetrySignalType::kScriptInjection;
    }

    // b. Protocol Handlers (XSS / Intent Detection)
    if ((base::EqualsCaseInsensitiveASCII(attr_name, "href") ||
         base::EqualsCaseInsensitiveASCII(attr_name, "src")) &&
        (base::StartsWith(
             attr_val, "javascript:", base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(attr_val,
                          "data:", base::CompareCase::INSENSITIVE_ASCII))) {
      return TelemetrySignalType::kScriptInjection;
    }
  }

  return TelemetrySignalType::kNone;
}

bool IsHighRiskEvent(TelemetrySignalType signal_type) {
  return signal_type != TelemetrySignalType::kNone;
}

bool IsActivityIncludedInTelemetry(const std::string& api_name,
                                   ActivityType type) {
  // DOM access is always included for deeper inspection.
  if (type == ActivityType::kDomAccess) {
    return true;
  }

  // For standard API calls, only include high-risk ones.
  if (type == ActivityType::kApiCall) {
    return api_name == "scripting.executeScript";
  }

  // Web Request activity, API events, and manifest content scripts
  // are not currently used by telemetry.
  return false;
}

std::vector<std::string> GetArgumentsList(const std::string& api_name,
                                          const base::ListValue& args_unsafe) {
  std::vector<std::string> arg_strings;
  const size_t args_size = args_unsafe.size();

  // Custom Extraction Rule: For blinkSetAttribute, we discard the 'old value'.
  // Raw format: [tag, attr_name, old_val, new_val]
  // Telemetry format: [tag, attr_name, new_val]
  if (api_name == "blinkSetAttribute" && args_size >= 4) {
    if (args_unsafe[0].is_string()) {
      arg_strings.push_back(args_unsafe[0].GetString());
    }
    if (args_unsafe[1].is_string()) {
      arg_strings.push_back(args_unsafe[1].GetString());
    }
    if (args_unsafe[3].is_string()) {
      arg_strings.push_back(args_unsafe[3].GetString());
    }
    return arg_strings;
  }

  // Custom Extraction Rule: For scripting.executeScript, we extract file names
  // or the actual function content.
  if (api_name == "scripting.executeScript" && args_size >= 1 &&
      args_unsafe[0].is_dict()) {
    const base::DictValue& dict = args_unsafe[0].GetDict();
    if (const base::ListValue* files = dict.FindList("files")) {
      for (const auto& file : *files) {
        if (file.is_string()) {
          arg_strings.push_back(file.GetString());
        }
      }
    }
    if (const std::string* func = dict.FindString("func")) {
      constexpr size_t kMaxFuncLength = 1024;
      if (func->length() > kMaxFuncLength) {
        arg_strings.push_back(func->substr(0, kMaxFuncLength));
      } else {
        arg_strings.push_back(*func);
      }
    }
    return arg_strings;
  }

  // Generic Extraction: Copy all string-typed arguments.
  for (const auto& arg : args_unsafe) {
    if (arg.is_string()) {
      arg_strings.push_back(arg.GetString());
    }
  }

  return arg_strings;
}

}  // namespace extensions::activity_log_policy_util
