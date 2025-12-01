// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains native implementation of AccountInfoSerializer.java
// methods.

#include "base/android/jni_string.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/public/android/test_support_jni_headers/AccountInfoSerializer_jni.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace signin {

static base::android::ScopedJavaLocalRef<jstring>
JNI_AccountInfoSerializer_AccountInfoToJsonString(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_account_info) {
  // Convert the Java AccountInfo to the C++ AccountInfo struct.
  AccountInfo account_info = ConvertFromJavaAccountInfo(env, j_account_info);

  // Call the static C++ function to serialize the AccountInfo.
  base::Value::Dict value_dict = signin::SerializeAccountInfo(account_info);

  // Convert the resulting base::Value::Dict to a JSON string.
  std::string json_string;
  if (!base::JSONWriter::Write(value_dict, &json_string)) {
    return nullptr;
  }

  // Convert the C++ string to a Java string and return it.
  return base::android::ConvertUTF8ToJavaString(env, json_string);
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_AccountInfoSerializer_JsonStringToAccountInfo(JNIEnv* env,
                                                  std::string& json_string) {
  // Parse the JSON string into a base::Value::Dict.
  std::optional<base::Value> value =
      base::JSONReader::Read(json_string, base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    return nullptr;
  }

  // Call the static C++ function to deserialize the AccountInfo.
  std::optional<AccountInfo> account_info =
      signin::DeserializeAccountInfo(value->GetDict());

  if (!account_info.has_value() || account_info->IsEmpty()) {
    return nullptr;
  }
  // Convert the C++ AccountInfo struct to a Java AccountInfo object and return
  // it.
  return ConvertToJavaAccountInfo(env, account_info.value());
}

}  // namespace signin

DEFINE_JNI(AccountInfoSerializer)
