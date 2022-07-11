// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied and modified from
// https://chromium.googlesource.com/chromium/src/+/a3f9d4fac81fc86065d867ab08fa4912ddf662c7/headless/public/internal/value_conversions.h
// Modifications include namespace and path.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVTOOLS_VALUE_CONVERSIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVTOOLS_VALUE_CONVERSIONS_H_

#include <memory>

#include "base/values.h"
#include "components/autofill_assistant/browser/devtools/error_reporter.h"

namespace autofill_assistant {
namespace internal {

// Generic conversion from a type to a base::Value. Implemented in
// types_DOMAIN.cc after all type-specific ToValueImpls have been defined.
template <typename T>
std::unique_ptr<base::Value> ToValue(const T& value);

// Generic conversion from a base::Value to a type. Note that this generic
// variant is never defined. Instead, we declare a specific template
// specialization for all the used types.
template <typename T>
struct FromValue {
  static std::unique_ptr<T> Parse(const base::Value& value,
                                  ErrorReporter* errors);
};

// ToValueImpl is a helper used by the ToValue template for dispatching into
// type-specific serializers. It uses a dummy |T*| argument as a way to
// partially specialize vector types.
template <typename T>
std::unique_ptr<base::Value> ToValueImpl(int value, T*) {
  return std::make_unique<base::Value>(value);
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(double value, T*) {
  return std::make_unique<base::Value>(value);
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(bool value, T*) {
  return std::make_unique<base::Value>(value);
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(const std::string& value, T*) {
  return std::make_unique<base::Value>(value);
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(const base::Value& value, T*) {
  return std::make_unique<base::Value>(value.Clone());
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(const std::vector<T>& vector,
                                         const std::vector<T>*) {
  base::Value::List result;
  for (const auto& it : vector)
    result.Append(base::Value::FromUniquePtrValue(ToValue(it)));
  return std::make_unique<base::Value>(std::move(result));
}

template <typename T>
std::unique_ptr<base::Value> ToValueImpl(const std::unique_ptr<T>& value,
                                         std::unique_ptr<T>*) {
  return ToValue(*value);
}

// FromValue specializations for basic types.
template <>
struct FromValue<bool> {
  static bool Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_bool()) {
      errors->AddError("boolean value expected");
      return false;
    }
    return value.GetBool();
  }
};

template <>
struct FromValue<int> {
  static int Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_int()) {
      errors->AddError("integer value expected");
      return 0;
    }
    return value.GetInt();
  }
};

template <>
struct FromValue<double> {
  static double Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_double() && !value.is_int()) {
      errors->AddError("double value expected");
      return 0;
    }
    return value.GetDouble();
  }
};

template <>
struct FromValue<std::string> {
  static std::string Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_string()) {
      errors->AddError("string value expected");
      return "";
    }
    return value.GetString();
  }
};

template <>
struct FromValue<base::DictionaryValue> {
  static std::unique_ptr<base::DictionaryValue> Parse(const base::Value& value,
                                                      ErrorReporter* errors) {
    const base::DictionaryValue* result;
    if (!value.GetAsDictionary(&result)) {
      errors->AddError("dictionary value expected");
      return nullptr;
    }
    return result->CreateDeepCopy();
  }
};

template <>
struct FromValue<base::Value> {
  static std::unique_ptr<base::Value> Parse(const base::Value& value,
                                            ErrorReporter* errors) {
    return base::Value::ToUniquePtrValue(value.Clone());
  }
};

template <typename T>
struct FromValue<std::unique_ptr<T>> {
  static std::unique_ptr<T> Parse(const base::Value& value,
                                  ErrorReporter* errors) {
    return FromValue<T>::Parse(value, errors);
  }
};

template <typename T>
struct FromValue<std::vector<T>> {
  static std::vector<T> Parse(const base::Value& value, ErrorReporter* errors) {
    std::vector<T> result;
    if (!value.is_list()) {
      errors->AddError("list value expected");
      return result;
    }
    errors->Push();
    for (const auto& item : value.GetList())
      result.push_back(FromValue<T>::Parse(item, errors));
    errors->Pop();
    return result;
  }
};

}  // namespace internal
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVTOOLS_VALUE_CONVERSIONS_H_
