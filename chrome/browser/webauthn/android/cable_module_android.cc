// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/cable_module_android.h"

#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_registration.h"
#include "device/fido/features.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

// This "header" actually contains function definitions and thus can only be
// included once across Chromium.
#include "chrome/browser/webauthn/android/jni_headers/CableAuthenticatorModuleProvider_jni.h"

using device::cablev2::authenticator::Registration;

namespace webauthn {
namespace authenticator {

namespace {

// kRootSecretPrefName is the name of a string preference that is kept in the
// browser's local state and which stores the base64-encoded root secret for
// the authenticator.
const char kRootSecretPrefName[] = "webauthn.authenticator_root_secret";

// RegistrationState is a singleton object that loads an install-wide secret at
// startup and holds two FCM registrations. One registration, the "linking"
// registration, is used when the user links with another device by scanning a
// QR code. The second is advertised via Sync for other devices signed into the
// same account. The reason for having two registrations is that the linking
// registration can be rotated if the user wishes to unlink all QR-linked
// devices. But we don't want to break synced peers when that happens. Instead,
// for synced peers we require that they have received a recent sync status from
// this device, i.e. we rotate them automatically.
class RegistrationState {
 public:
  void Register() {
    DCHECK(!linking_registration_);
    DCHECK(!sync_registration_);

    instance_id::InstanceIDDriver* const driver =
        instance_id::InstanceIDProfileServiceFactory::GetForProfile(
            g_browser_process->profile_manager()->GetPrimaryUserProfile())
            ->driver();
    linking_registration_ = device::cablev2::authenticator::Register(
        driver, device::cablev2::authenticator::Registration::Type::LINKING,
        base::BindOnce(&RegistrationState::OnLinkingRegistrationReady,
                       base::Unretained(this)),
        base::BindRepeating(&RegistrationState::OnEvent,
                            base::Unretained(this)));
    sync_registration_ = device::cablev2::authenticator::Register(
        driver, device::cablev2::authenticator::Registration::Type::SYNC,
        base::BindOnce(&RegistrationState::OnSyncRegistrationReady,
                       base::Unretained(this)),
        base::BindRepeating(&RegistrationState::OnEvent,
                            base::Unretained(this)));

    sync_registration_->PrepareContactID();

    PrefService* const local_state = g_browser_process->local_state();
    std::string secret_base64 = local_state->GetString(kRootSecretPrefName);

    if (!secret_base64.empty()) {
      std::string secret_str;
      if (base::Base64Decode(secret_base64, &secret_str) &&
          secret_str.size() == secret_.size()) {
        memcpy(secret_.data(), secret_str.data(), secret_.size());
      } else {
        secret_base64.clear();
      }
    }

    if (secret_base64.empty()) {
      crypto::RandBytes(secret_);
      local_state->SetString(kRootSecretPrefName, base::Base64Encode(secret_));
    }
  }

  bool is_registered_for_linking() const {
    return linking_registration_ != nullptr;
  }
  bool is_registered_for_sync() const { return sync_registration_ != nullptr; }

  Registration* linking_registration() const {
    return linking_registration_.get();
  }

  Registration* sync_registration() const { return sync_registration_.get(); }

  const std::array<uint8_t, 32>& secret() const { return secret_; }

  // have_data_for_sync returns true if this object has loaded enough state to
  // put information into sync's DeviceInfo.
  bool have_data_for_sync() const {
    return sync_registration_ != nullptr && sync_registration_->contact_id();
  }

  void set_signal_sync_when_ready() { signal_sync_when_ready_ = true; }

 private:
  void OnLinkingRegistrationReady() { MaybeFlushPendingEvent(); }

  void OnSyncRegistrationReady() { MaybeSignalSync(); }

  // OnEvent is called when a GCM message is received.
  void OnEvent(std::unique_ptr<Registration::Event> event) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    pending_event_ = std::move(event);

    MaybeFlushPendingEvent();
  }

  void MaybeFlushPendingEvent() {
    if (!pending_event_) {
      return;
    }

    if (pending_event_->source == Registration::Type::LINKING &&
        !linking_registration_->contact_id()) {
      // This GCM message is from a QR-linked peer so it needs the contact ID to
      // be processed, but that contact ID isn't ready yet.
      linking_registration_->PrepareContactID();
      return;
    }

    Java_CableAuthenticatorModuleProvider_onCloudMessage(
        base::android::AttachCurrentThread(),
        static_cast<jlong>(
            reinterpret_cast<uintptr_t>(pending_event_.release())));
  }

  // MaybeSignalSync prompts the Sync system to refresh local-device data if
  // the Sync data is now ready and |signal_sync_when_ready_| has been set to
  // indicate that the Sync data was not available last time Sync queried it.
  void MaybeSignalSync() {
    if (!signal_sync_when_ready_ || !have_data_for_sync()) {
      return;
    }
    signal_sync_when_ready_ = false;

    DeviceInfoSyncServiceFactory::GetForProfile(
        ProfileManager::GetPrimaryUserProfile())
        ->RefreshLocalDeviceInfo();
  }

  std::unique_ptr<Registration> linking_registration_;
  std::unique_ptr<Registration> sync_registration_;
  std::array<uint8_t, 32> secret_;
  std::unique_ptr<Registration::Event> pending_event_;
  bool signal_sync_when_ready_ = false;
};

RegistrationState* GetRegistrationState() {
  static base::NoDestructor<RegistrationState> state;
  return state.get();
}

}  // namespace

void RegisterForCloudMessages() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!base::FeatureList::IsEnabled(device::kWebAuthCableSecondFactor) &&
      !base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    return;
  }

  GetRegistrationState()->Register();
}

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kRootSecretPrefName, std::string());
}

base::Optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>
GetSyncDataIfRegistered() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!base::FeatureList::IsEnabled(device::kWebAuthCableSecondFactor)) {
    return base::nullopt;
  }

  RegistrationState* state = GetRegistrationState();
  if (!state->have_data_for_sync()) {
    // Not yet ready to provide sync data. When the data is ready,
    // |state| will signal to Sync that something changed and this
    // function will be called again.
    state->set_signal_sync_when_ready();
    return base::nullopt;
  }

  syncer::DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
  paask_info.tunnel_server_domain = device::cablev2::kTunnelServer;
  paask_info.contact_id = *state->sync_registration()->contact_id();
  const uint32_t pairing_id = device::cablev2::sync::IDNow();
  paask_info.id = pairing_id;

  std::array<uint8_t, device::cablev2::kPairingIDSize> pairing_id_bytes = {0};
  static_assert(sizeof(pairing_id) <= EXTENT(pairing_id_bytes), "");
  memcpy(pairing_id_bytes.data(), &pairing_id, sizeof(pairing_id));

  paask_info.secret = device::cablev2::Derive<EXTENT(paask_info.secret)>(
      state->secret(), pairing_id_bytes,
      device::cablev2::DerivedValueType::kPairedSecret);

  bssl::UniquePtr<EC_KEY> identity_key =
      device::cablev2::IdentityKey(state->secret());
  CHECK_EQ(paask_info.peer_public_key_x962.size(),
           EC_POINT_point2oct(EC_KEY_get0_group(identity_key.get()),
                              EC_KEY_get0_public_key(identity_key.get()),
                              POINT_CONVERSION_UNCOMPRESSED,
                              paask_info.peer_public_key_x962.data(),
                              paask_info.peer_public_key_x962.size(),
                              /*ctx=*/nullptr));

  return paask_info;
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
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(
      webauthn::authenticator::GetRegistrationState()->linking_registration()));
}

static void JNI_CableAuthenticatorModuleProvider_FreeEvent(JNIEnv* env,
                                                           jlong event_long) {
  static_assert(sizeof(jlong) >= sizeof(uintptr_t),
                "Java longs are too small to contain pointers");
  Registration::Event* event =
      reinterpret_cast<Registration::Event*>(event_long);
  delete event;
}

static base::android::ScopedJavaLocalRef<jbyteArray>
JNI_CableAuthenticatorModuleProvider_GetSecret(JNIEnv* env) {
  return base::android::ToJavaByteArray(
      env, webauthn::authenticator::GetRegistrationState()->secret());
}
