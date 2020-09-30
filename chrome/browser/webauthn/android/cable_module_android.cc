// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"

// This "header" actually contains function definitions and thus can only be
// included once across Chromium.
#include "chrome/browser/webauthn/android/jni_headers/CableAuthenticatorModuleProvider_jni.h"

static jlong JNI_CableAuthenticatorModuleProvider_GetSystemNetworkContext(
    JNIEnv* env) {
  static_assert(sizeof(jlong) >= sizeof(uintptr_t),
                "Java longs are too small to contain pointers");
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(
      SystemNetworkContextManager::GetInstance()->GetContext()));
}

static jlong JNI_CableAuthenticatorModuleProvider_GetInstanceIDDriver(
    JNIEnv* env) {
  static_assert(sizeof(jlong) >= sizeof(uintptr_t),
                "Java longs are too small to contain pointers");
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(
          g_browser_process->profile_manager()->GetPrimaryUserProfile())
          ->driver()));
}
