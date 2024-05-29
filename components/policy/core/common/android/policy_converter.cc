// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/android/policy_converter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/policy/android/jni_headers/PolicyConverter_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaRef;

namespace policy {
namespace android {

namespace {

// Tries to parse lists as comma-separated values. Extra spaces are ignored, so
// "foo,bar" and "foo, bar" are equivalent. This is best effort and intended to
// cover common cases applicable to the majority of policies. Use JSON encoding
// to handle corner cases not covered by this.
std::optional<base::Value> SplitCommaSeparatedList(
    const std::string& str_value) {
  DCHECK(!str_value.empty());

  base::Value::List as_list;
  std::vector<std::string> items_as_vector = base::SplitString(
      str_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::ranges::for_each(items_as_vector, [&as_list](const std::string& item) {
    as_list.Append(base::Value(item));
  });
  return base::Value(std::move(as_list));
}

}  // namespace

PolicyConverter::PolicyConverter(const Schema* policy_schema)
    : policy_schema_(policy_schema) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env,
      Java_PolicyConverter_create(env, reinterpret_cast<intptr_t>(this)).obj());
  DCHECK(!java_obj_.is_null());
}

PolicyConverter::~PolicyConverter() {
  Java_PolicyConverter_onNativeDestroyed(base::android::AttachCurrentThread(),
                                         java_obj_);
}

PolicyBundle PolicyConverter::GetPolicyBundle() {
  PolicyBundle filled_bundle = std::move(policy_bundle_);
  policy_bundle_ = PolicyBundle();
  return filled_bundle;
}

base::android::ScopedJavaLocalRef<jobject> PolicyConverter::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void PolicyConverter::SetPolicyBoolean(JNIEnv* env,
                                       const JavaRef<jobject>& obj,
                                       const JavaRef<jstring>& policyKey,
                                       jboolean value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 base::Value(static_cast<bool>(value)));
}

void PolicyConverter::SetPolicyInteger(JNIEnv* env,
                                       const JavaRef<jobject>& obj,
                                       const JavaRef<jstring>& policyKey,
                                       jint value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 base::Value(static_cast<int>(value)));
}

void PolicyConverter::SetPolicyString(JNIEnv* env,
                                      const JavaRef<jobject>& obj,
                                      const JavaRef<jstring>& policyKey,
                                      const JavaRef<jstring>& value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 base::Value(ConvertJavaStringToUTF8(env, value)));
}

void PolicyConverter::SetPolicyStringArray(JNIEnv* env,
                                           const JavaRef<jobject>& obj,
                                           const JavaRef<jstring>& policyKey,
                                           const JavaRef<jobjectArray>& array) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 base::Value(ConvertJavaStringArrayToListValue(env, array)));
}

// static
base::Value::List PolicyConverter::ConvertJavaStringArrayToListValue(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array) {
  DCHECK(!array.is_null());
  base::android::JavaObjectArrayReader<jstring> array_reader(array);
  DCHECK_GE(array_reader.size(), 0)
      << "Invalid array length: " << array_reader.size();

  base::Value::List list_value;
  for (auto j_str : array_reader)
    list_value.Append(ConvertJavaStringToUTF8(env, j_str));

  return list_value;
}

// static
std::optional<base::Value> PolicyConverter::ConvertValueToSchema(
    base::Value value,
    const Schema& schema) {
  if (!schema.valid())
    return value;

  switch (schema.type()) {
    case base::Value::Type::NONE:
      return base::Value();

    case base::Value::Type::BOOLEAN: {
      if (value.is_string()) {
        const std::string& string_value = value.GetString();
        if (string_value.compare("true") == 0)
          return base::Value(true);

        if (string_value.compare("false") == 0)
          return base::Value(false);

        return value;
      }
      if (value.is_int())
        return base::Value(value.GetInt() != 0);

      return value;
    }

    case base::Value::Type::INTEGER: {
      if (value.is_string()) {
        const std::string& string_value = value.GetString();
        int int_value = 0;
        if (base::StringToInt(string_value, &int_value))
          return base::Value(int_value);
      }
      return value;
    }

    case base::Value::Type::DOUBLE: {
      if (value.is_string()) {
        const std::string& string_value = value.GetString();
        double double_value = 0;
        if (base::StringToDouble(string_value, &double_value))
          return base::Value(double_value);
      }
      return value;
    }

    // String can't be converted from other types.
    case base::Value::Type::STRING: {
      return value;
    }

    // Binary is not a valid schema type.
    case base::Value::Type::BINARY: {
      NOTREACHED_IN_MIGRATION();
      return base::Value();
    }

    // Complex types have to be deserialized from JSON.
    case base::Value::Type::DICT: {
      if (value.is_string()) {
        const std::string str_value = value.GetString();
        // Do not try to convert empty string to list/dictionaries, since most
        // likely the value was not simply not set by the UEM.
        if (str_value.empty()) {
          return std::nullopt;
        }
        std::optional<base::Value> decoded_value = base::JSONReader::Read(
            str_value, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
        if (decoded_value) {
          return decoded_value;
        }
      }
      return value;
    }

    case base::Value::Type::LIST: {
      if (value.is_string()) {
        const std::string str_value = value.GetString();
        // Do not try to convert empty string to list/dictionaries, since most
        // likely the value was not simply not set by the UEM.
        if (str_value.empty()) {
          return std::nullopt;
        }
        std::optional<base::Value> decoded_value = base::JSONReader::Read(
            str_value, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
        return decoded_value ? std::move(decoded_value)
                             : SplitCommaSeparatedList(str_value);
      }
      return value;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

void PolicyConverter::SetPolicyValueForTesting(const std::string& key,
                                               base::Value value) {
  PolicyConverter::SetPolicyValue(key, std::move(value));
}

void PolicyConverter::SetPolicyValue(const std::string& key,
                                     base::Value value) {
  const Schema schema = policy_schema_->GetKnownProperty(key);
  const PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  std::optional<base::Value> converted_value =
      ConvertValueToSchema(std::move(value), schema);
  if (converted_value) {
    // Do not set list/dictionary policies that are sent as empty strings from
    // the UEM. This is common on Android when the UEM pushes the policy with
    // managed configurations.
    policy_bundle_.Get(ns).Set(key, POLICY_LEVEL_MANDATORY,
                               POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                               std::move(converted_value), nullptr);
  }
}

}  // namespace android
}  // namespace policy
