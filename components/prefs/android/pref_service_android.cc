// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/android/pref_service_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/prefs_export.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/prefs/android/jni_headers/PrefService_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

PrefServiceAndroid::PrefServiceAndroid(PrefService* pref_service)
    : pref_service_(pref_service) {}

PrefServiceAndroid::~PrefServiceAndroid() {
  if (java_ref_) {
    Java_PrefService_clearNativePtr(AttachCurrentThread(), java_ref_);
    java_ref_.Reset();
  }
}

// static
PrefService* PrefServiceAndroid::FromPrefServiceAndroid(
    const JavaRef<jobject>& obj) {
  if (obj.is_null()) {
    return nullptr;
  }

  PrefServiceAndroid* pref_service_android =
      reinterpret_cast<PrefServiceAndroid*>(
          Java_PrefService_getNativePointer(AttachCurrentThread(), obj));
  if (!pref_service_android) {
    return nullptr;
  }
  return pref_service_android->pref_service_;
}

ScopedJavaLocalRef<jobject> PrefServiceAndroid::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  if (!java_ref_) {
    java_ref_.Reset(
        Java_PrefService_create(env, reinterpret_cast<intptr_t>(this)));
  }
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

void PrefServiceAndroid::ClearPref(const std::string& preference) {
  pref_service_->ClearPref(preference);
}

bool PrefServiceAndroid::HasPrefPath(const std::string& preference) {
  return pref_service_->HasPrefPath(preference);
}

bool PrefServiceAndroid::GetBoolean(const std::string& preference) {
  return pref_service_->GetBoolean(preference);
}

void PrefServiceAndroid::SetBoolean(const std::string& preference, bool value) {
  pref_service_->SetBoolean(preference, value);
}

int32_t PrefServiceAndroid::GetInteger(const std::string& preference) {
  return pref_service_->GetInteger(preference);
}

void PrefServiceAndroid::SetInteger(const std::string& preference,
                                    int32_t value) {
  pref_service_->SetInteger(preference, value);
}

double PrefServiceAndroid::GetDouble(const std::string& preference) {
  return pref_service_->GetDouble(preference);
}

void PrefServiceAndroid::SetDouble(const std::string& preference,
                                   double value) {
  pref_service_->SetDouble(preference, value);
}

int64_t PrefServiceAndroid::GetLong(const std::string& preference) {
  return pref_service_->GetInt64(preference);
}

void PrefServiceAndroid::SetLong(const std::string& preference, int64_t value) {
  pref_service_->SetInt64(preference, value);
}

std::string PrefServiceAndroid::GetString(const std::string& preference) {
  return pref_service_->GetString(preference);
}

void PrefServiceAndroid::SetString(const std::string& preference,
                                   const std::string& value) {
  pref_service_->SetString(preference, value);
}

bool PrefServiceAndroid::IsManagedPreference(const std::string& preference) {
  return pref_service_->IsManagedPreference(preference);
}

bool PrefServiceAndroid::HasRecommendation(const std::string& preference) {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(preference);
  return pref && pref->GetRecommendedValue() != nullptr;
}

bool PrefServiceAndroid::IsFollowingRecommendation(
    const std::string& preference) {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(preference);
  return pref && pref->GetRecommendedValue() &&
         *pref->GetRecommendedValue() == *pref->GetValue();
}

bool PrefServiceAndroid::IsRecommendedPreference(
    const std::string& preference) {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(preference);
  return pref && pref->IsRecommended();
}

bool PrefServiceAndroid::IsDefaultValuePreference(
    const std::string& preference) {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(preference);
  return pref && pref->IsDefaultValue();
}

namespace jni_zero {

template <>
ScopedJavaLocalRef<jobject> ToJniType<PrefService>(JNIEnv* env,
                                                   PrefService* pref_service) {
  return pref_service->GetJavaObject();
}
}  // namespace jni_zero

DEFINE_JNI(PrefService)
