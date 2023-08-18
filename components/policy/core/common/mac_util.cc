// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mac_util.h"

#include <memory>
#include <string>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

using base::apple::CFCast;

namespace policy {

namespace {

// Callback function for CFDictionaryApplyFunction. |key| and |value| are an
// entry of the CFDictionary that should be converted into an equivalent entry
// in the DictionaryValue in |context|.
void DictionaryEntryToValue(const void* key, const void* value, void* context) {
  if (CFStringRef cf_key = CFCast<CFStringRef>(key)) {
    std::unique_ptr<base::Value> converted =
        PropertyToValue(static_cast<CFPropertyListRef>(value));
    if (converted) {
      const std::string string = base::SysCFStringRefToUTF8(cf_key);
      // Policy dictionary values may contain dots in key names.
      static_cast<base::Value::Dict*>(context)->Set(string,
                                                    std::move(*converted));
    }
  }
}

// Callback function for CFArrayApplyFunction. |value| is an entry of the
// CFArray that should be converted into an equivalent entry in the ListValue
// in |context|.
void ArrayEntryToValue(const void* value, void* context) {
  std::unique_ptr<base::Value> converted =
      PropertyToValue(static_cast<CFPropertyListRef>(value));
  if (converted) {
    static_cast<base::Value::List*>(context)->Append(
        base::Value::FromUniquePtrValue(std::move(converted)));
  }
}

}  // namespace

std::unique_ptr<base::Value> PropertyToValue(CFPropertyListRef property) {
  if (CFCast<CFNullRef>(property))
    return std::make_unique<base::Value>();

  if (CFBooleanRef boolean = CFCast<CFBooleanRef>(property)) {
    return std::make_unique<base::Value>(
        static_cast<bool>(CFBooleanGetValue(boolean)));
  }

  if (CFNumberRef number = CFCast<CFNumberRef>(property)) {
    // CFNumberGetValue() converts values implicitly when the conversion is not
    // lossy. Check the type before trying to convert.
    if (CFNumberIsFloatType(number)) {
      double double_value = 0.0;
      if (CFNumberGetValue(number, kCFNumberDoubleType, &double_value)) {
        return std::make_unique<base::Value>(double_value);
      }
    } else {
      int int_value = 0;
      if (CFNumberGetValue(number, kCFNumberIntType, &int_value)) {
        return std::make_unique<base::Value>(int_value);
      }
    }
  }

  if (CFStringRef string = CFCast<CFStringRef>(property)) {
    return std::make_unique<base::Value>(base::SysCFStringRefToUTF8(string));
  }

  if (CFDictionaryRef dict = CFCast<CFDictionaryRef>(property)) {
    base::Value::Dict dict_value;
    CFDictionaryApplyFunction(dict, DictionaryEntryToValue, &dict_value);
    return std::make_unique<base::Value>(std::move(dict_value));
  }

  if (CFArrayRef array = CFCast<CFArrayRef>(property)) {
    base::Value::List list_value;
    CFArrayApplyFunction(array, CFRangeMake(0, CFArrayGetCount(array)),
                         ArrayEntryToValue, &list_value);
    return std::make_unique<base::Value>(std::move(list_value));
  }

  return nullptr;
}

}  // namespace policy
