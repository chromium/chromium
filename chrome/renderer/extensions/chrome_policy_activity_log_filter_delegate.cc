// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_policy_activity_log_filter_delegate.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/common/dom_action_types.h"
#include "url/gurl.h"

namespace extensions {

ChromePolicyActivityLogFilterDelegate::ChromePolicyActivityLogFilterDelegate() =
    default;
ChromePolicyActivityLogFilterDelegate::
    ~ChromePolicyActivityLogFilterDelegate() = default;

bool ChromePolicyActivityLogFilterDelegate::IsHighRiskEvent(
    const ExtensionId& extension_id,
    DomActionType::Type action_type,
    const std::string& api_name,
    const base::ListValue& args,
    const GURL& url) {
  // --- CATEGORY 1: CONFIDENTIALITY SIGNALS (Data Theft) ---
  if (api_name == "Document.cookie" || api_name == "HTMLInputElement.value" ||
      api_name == "HTMLTextAreaElement.value") {
    return true;
  }

  // --- CATEGORY 2: INTEGRITY SIGNALS (Injection Defense) ---
  bool is_set_attr = (api_name == "blinkSetAttribute");
  bool is_add_elem = (api_name == "blinkAddElement");

  if (!is_set_attr && !is_add_elem) {
    return false;
  }

  // Pre-processing: Identify tag and attribute context.
  // For these manual hooks, the tag name is always provided in args[0].
  if (args.empty() || !args[0].is_string()) {
    return false;
  }

  // Extract the tag from the first argument.
  const std::string& tag = args[0].GetString();

  // A. Remote Script Injection (The "Dropper" Check)
  if (base::EqualsCaseInsensitiveASCII(tag, "script")) {
    if (is_add_elem) {
      // blinkAddElement: [tag="script", src]
      return true;
    }
    if (is_set_attr && args.size() >= 4 && args[1].is_string() &&
        base::EqualsCaseInsensitiveASCII(args[1].GetString(), "src")) {
      // blinkSetAttribute: [tag="script", attr="src", old_val, new_val]
      return true;
    }
  }

  // B. Executable Element Creation
  if (is_add_elem && base::EqualsCaseInsensitiveASCII(tag, "iframe")) {
    // blinkAddElement: [tag="iframe", src]
    return true;
  }

  // C. Attribute-Based Injection (Unified Logic)
  std::string_view attr_name;
  std::string_view attr_val;

  if (is_set_attr && args.size() >= 4) {
    // Explicit attribute change.
    // blinkSetAttribute format: [tag, attr_name, old_val, new_val]
    if (args[1].is_string()) {
      attr_name = args[1].GetString();
    }
    if (args[3].is_string()) {
      attr_val = args[3].GetString();
    }
  } else if (is_add_elem && args.size() >= 2) {
    // Element creation with attributes in payload.
    // blinkAddElement formats vary by element:
    if (base::EqualsCaseInsensitiveASCII(tag, "a")) {
      // ["a", href]
      attr_name = "href";
      if (args[1].is_string()) {
        attr_val = args[1].GetString();
      }
    } else if (base::EqualsCaseInsensitiveASCII(tag, "link")) {
      // ["link", rel, href]
      if (args.size() >= 3) {
        attr_name = "href";
        if (args[2].is_string()) {
          attr_val = args[2].GetString();
        }
      }
    } else if (base::EqualsCaseInsensitiveASCII(tag, "form")) {
      // ["form", method, action]
      if (args.size() >= 3) {
        attr_name = "action";
        if (args[2].is_string()) {
          attr_val = args[2].GetString();
        }
      }
    } else if (base::EqualsCaseInsensitiveASCII(tag, "input") ||
               base::EqualsCaseInsensitiveASCII(tag, "button")) {
      // [tag, type, formaction]
      if (args.size() >= 3) {
        attr_name = "formaction";
        if (args[2].is_string()) {
          attr_val = args[2].GetString();
        }
      }
    }
  }

  if (!attr_name.empty()) {
    // 1. Form Hijacking (Phishing)
    if (base::EqualsCaseInsensitiveASCII(attr_name, "action") ||
        base::EqualsCaseInsensitiveASCII(attr_name, "formaction")) {
      return true;
    }

    // 2. Protocol Handlers (XSS / Intent Detection)
    if ((base::EqualsCaseInsensitiveASCII(attr_name, "href") ||
         base::EqualsCaseInsensitiveASCII(attr_name, "src")) &&
        (base::StartsWith(
             attr_val, "javascript:", base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(attr_val,
                          "data:", base::CompareCase::INSENSITIVE_ASCII))) {
      return true;
    }
  }

  return false;
}

}  // namespace extensions
