// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "content/public/browser/browser_thread.h"
#include "device/fido/cable/v2_registration.h"

// This "header" actually contains function definitions and thus can only be
// included once across Chromium.
#include "chrome/browser/webauthn/android/jni_headers/CableAuthenticatorModuleProvider_jni.h"

using device::cablev2::authenticator::Registration;

namespace webauthn {
namespace authenticator {

namespace {

// OnEvent is called when a GCM message is received.
void OnEvent(std::unique_ptr<Registration::Event> event) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  Java_CableAuthenticatorModuleProvider_onCloudMessage(
      base::android::AttachCurrentThread(),
      static_cast<jlong>(reinterpret_cast<uintptr_t>(event.release())));
}

// RegistrationHolder owns a |Registration|. It's needed because we don't want
// to have to gather the arguments for |base::NoDestructor| each time because
// they usually won't be needed.
class RegistrationHolder {
 public:
  RegistrationHolder() {
    instance_id::InstanceIDDriver* const driver =
        instance_id::InstanceIDProfileServiceFactory::GetForProfile(
            g_browser_process->profile_manager()->GetPrimaryUserProfile())
            ->driver();
    registration_ = device::cablev2::authenticator::Register(
        driver, base::BindRepeating(OnEvent));
  }

  Registration* get() const { return registration_.get(); }

 private:
  std::unique_ptr<Registration> registration_;
};

Registration* GetRegistration() {
  static base::NoDestructor<RegistrationHolder> registration;
  return registration->get();
}

}  // namespace

void RegisterForCloudMessages() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetRegistration();
}

}  // namespace authenticator
}  // namespace webauthn

// JNI callbacks.

static jlong JNI_CableAuthenticatorModuleProvider_GetSystemNetworkContext(
    JNIEnv* env) {
  static_assert(sizeof(jlong) >= sizeof(uintptr_t),
                "Java longs are too small to contain pointers");
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(
      SystemNetworkContextManager::GetInstance()->GetContext()));
}

static jlong JNI_CableAuthenticatorModuleProvider_GetRegistration(JNIEnv* env) {
  static_assert(sizeof(jlong) >= sizeof(uintptr_t),
                "Java longs are too small to contain pointers");
  return static_cast<jlong>(
      reinterpret_cast<uintptr_t>(webauthn::authenticator::GetRegistration()));
}

static void JNI_CableAuthenticatorModuleProvider_FreeEvent(JNIEnv* env,
                                                           jlong event_long) {
  static_assert(sizeof(jlong) >= sizeof(uintptr_t),
                "Java longs are too small to contain pointers");
  Registration::Event* event =
      reinterpret_cast<Registration::Event*>(event_long);
  delete event;
}
