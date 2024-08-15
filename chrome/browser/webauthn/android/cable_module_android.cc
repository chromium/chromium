// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/webauthn/android/cable_module_android.h"

#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/webauthn/android/cable_registration_state.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_registration.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/features.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

// These "headers" actually contains function definitions and thus can only be
// included once across Chromium.
#include "chrome/browser/webauthn/android/jni_headers/CableAuthenticatorModuleProvider_jni.h"
#include "chrome/browser/webauthn/android/jni_headers/PrivacySettingsFragment_jni.h"

using device::cablev2::authenticator::Registration;

namespace webauthn {
namespace authenticator {

namespace {

// kRootSecretPrefName is the name of a string preference that is kept in the
// browser's local state and which stores the base64-encoded root secret for
// the authenticator.
const char kRootSecretPrefName[] = "webauthn.authenticator_root_secret";
const char kSerializedPaaskFieldsName[] = "webauthn.authenticator_info";

const char kWorkProfilePrefName[] = "webauthn.in_work_profile";
// kWorkProfilePrefName wants to be a tristate. Since there's no support for
// that in `PrefService`, it's simulated with a string that is empty if unset,
// and takes one of the following values when set.
const char kInWorkProfile[] = "1";
const char kNotInWorkProfile[] = "0";

// SystemInterface connects a `RegistrationState` to the rest of the system.
// This object is owned by the `RegistrationState`, and that is a singleton
// object. So this object is a singleton too and so can do things like pass a
// pointer to itself to Java functions to route the eventual callback.
class SystemInterface : public RegistrationState::SystemInterface {
 public:
  std::unique_ptr<device::cablev2::authenticator::Registration> NewRegistration(
      device::cablev2::authenticator::Registration::Type type,
      base::OnceCallback<void()> on_ready,
      base::RepeatingCallback<void(
          std::unique_ptr<device::cablev2::authenticator::Registration::Event>)>
          event_callback) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    return device::cablev2::authenticator::Register(
        GetDriver(), type, std::move(on_ready), std::move(event_callback));
  }

  std::string GetRootSecret() override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    return g_browser_process->local_state()->GetString(kRootSecretPrefName);
  }

  void SetRootSecret(std::string secret) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    g_browser_process->local_state()->SetString(kRootSecretPrefName,
                                                std::move(secret));
  }

  void CanDeviceSupportCable(base::OnceCallback<void(bool)> callback) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &SystemInterface::GetCanDeviceSupportCableOnBackgroundSequence),
        std::move(callback));
  }

  void AmInWorkProfile(base::OnceCallback<void(bool)> callback) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    // Checking whether an app is in a work profile is costly. We assume that a
    // given Chrome profile never moves between being in a work profile or not
    // and thus cache the result on disk.
    const std::string work_profile_state =
        g_browser_process->local_state()->GetString(kWorkProfilePrefName);
    if (work_profile_state == kInWorkProfile) {
      std::move(callback).Run(true);
    } else if (work_profile_state == kNotInWorkProfile) {
      std::move(callback).Run(false);
    } else {
      work_profile_callback_ = std::move(callback);
      // Checking whether this Chrome is in a work profile is sufficiently
      // expensive that doing it at startup impacts benchmarks. (See
      // crbug.com/1459794.) Since startup is an especially contended time, we
      // wait a few minutes before doing this check.
      content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
          ->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&SystemInterface::GetWorkProfileStatus,
                             base::Unretained(this)),
              base::Minutes(3));
    }
  }

  void CalculateIdentityKey(
      const std::array<uint8_t, 32>& secret,
      base::OnceCallback<void(bssl::UniquePtr<EC_KEY>)> callback) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &SystemInterface::CalculateIdentityKeyOnBackgroundSequence, secret),
        std::move(callback));
  }

  void GetPrelinkFromPlayServices(
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback)
      override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!prelink_callback_);
    prelink_callback_ = std::move(callback);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &SystemInterface::GetPrelinkFromPlayServicesOnBackgroundSequence,
            // Passing this pointer is reasonable because this object is owned
            // by a singleton.
            reinterpret_cast<uintptr_t>(this)));
  }

  void OnCloudMessage(std::vector<uint8_t> serialized,
                      bool is_make_credential) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    JNIEnv* const env = base::android::AttachCurrentThread();
    Java_CableAuthenticatorModuleProvider_onCloudMessage(
        env, base::android::ToJavaByteArray(env, serialized),
        is_make_credential);
  }

  void RefreshLocalDeviceInfo() override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DeviceInfoSyncServiceFactory::GetForProfile(
        ProfileManager::GetPrimaryUserProfile())
        ->RefreshLocalDeviceInfo();
  }

  // Called when the Java code has finished getting linking information from
  // Play Services.
  void OnHavePlayServicesLinkingInformation(
      std::optional<std::vector<uint8_t>> cbor) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    std::move(prelink_callback_).Run(std::move(cbor));
  }

  // Called when the Java code has finished checking if we're running in a work
  // profile.
  void OnHaveWorkProfileResult(bool in_work_profile) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    g_browser_process->local_state()->SetString(
        kWorkProfilePrefName,
        in_work_profile ? kInWorkProfile : kNotInWorkProfile);

    std::move(work_profile_callback_).Run(in_work_profile);
  }

 private:
  static instance_id::InstanceIDDriver* GetDriver() {
    return instance_id::InstanceIDProfileServiceFactory::GetForProfile(
               g_browser_process->profile_manager()->GetPrimaryUserProfile())
        ->driver();
  }

  static bool GetCanDeviceSupportCableOnBackgroundSequence() {
    // This runs on a worker thread because this Java function can take a
    // little while and it shouldn't block the UI thread.
    return Java_CableAuthenticatorModuleProvider_canDeviceSupportCable(
        base::android::AttachCurrentThread());
  }

  static bssl::UniquePtr<EC_KEY> CalculateIdentityKeyOnBackgroundSequence(
      std::array<uint8_t, 32> secret) {
    // This runs on a worker thread because the scalar multiplication takes a
    // few milliseconds on slower devices.
    return device::cablev2::IdentityKey(secret);
  }

  static void GetPrelinkFromPlayServicesOnBackgroundSequence(
      uintptr_t this_pointer) {
    // This runs on a worker thread because this Java function can take a
    // little while and it shouldn't block the UI thread.
    Java_CableAuthenticatorModuleProvider_getLinkingInformation(
        base::android::AttachCurrentThread(), this_pointer);
  }

  void GetWorkProfileStatus() {
    // This Java function must run on the UI thread, but that's ok because it
    // defers work to a worker thread itself. It returns its result by calling
    // `OnHaveWorkProfileResult` on this object.
    Java_CableAuthenticatorModuleProvider_amInWorkProfile(
        base::android::AttachCurrentThread(),
        reinterpret_cast<uintptr_t>(this));
  }

  base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
      prelink_callback_;
  base::OnceCallback<void(bool)> work_profile_callback_;
};

RegistrationState* GetRegistrationState() {
  static base::NoDestructor<RegistrationState> state(
      std::make_unique<SystemInterface>());
  return state.get();
}

using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::Map;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;

// PreLinkInfo reflects the linking information provided by Play Services.
struct PreLinkInfo {
  // RAW_PTR_EXCLUSION: cbor_extract.cc would cast the raw_ptr<T> to a void*,
  // skipping an AddRef() call and causing a ref-counting mismatch.
  RAW_PTR_EXCLUSION const std::vector<uint8_t>* contact_id;
  RAW_PTR_EXCLUSION const std::vector<uint8_t>* pairing_id;
  RAW_PTR_EXCLUSION const std::vector<uint8_t>* secret;
  RAW_PTR_EXCLUSION const std::vector<uint8_t>* peer_public_key_x962;
};

// kPreLinkInfoSteps contains parsing instructions for cbor_extract to convert
// the CBOR-encoded data from Play Services into a `PreLinkInfo`. The format
// that Play Services uses mostly follows the "linking map" structure defined in
// https://fidoalliance.org/specs/fido-v2.2-rd-20230321/fido-client-to-authenticator-protocol-v2.2-rd-20230321.html#hybrid-qr-initiated
static constexpr StepOrByte<PreLinkInfo> kPreLinkInfoSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, PreLinkInfo, contact_id),
    IntKey<PreLinkInfo>(1),
    ELEMENT(Is::kRequired, PreLinkInfo, pairing_id),
    IntKey<PreLinkInfo>(2),
    ELEMENT(Is::kRequired, PreLinkInfo, secret),
    IntKey<PreLinkInfo>(3),
    ELEMENT(Is::kRequired, PreLinkInfo, peer_public_key_x962),
    IntKey<PreLinkInfo>(4),

    Stop<PreLinkInfo>(),
    // clang-format on
};

syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
GetSyncDataIfRegisteredInternal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  RegistrationState* state = GetRegistrationState();
  if (!state->have_data_for_sync()) {
    // Not yet ready to provide sync data. When the data is ready,
    // |state| will signal to Sync that something changed and this
    // function will be called again.
    state->SignalSyncWhenReady();
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NotReady();
  }

  if (state->am_in_work_profile()) {
    // Never publish pre-linking information when in a work profile, instead
    // route hybrid requests into the main profile.
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
  }

  if (state->link_data_from_play_services()) {
    std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> paask_info =
        internal::PaaskInfoFromCBOR(*state->link_data_from_play_services());
    if (paask_info) {
      return *paask_info;
    } else {
      LOG(ERROR)
          << "Failed to parse PaaSK prelink information from Play Services";
    }
  }

  if (!base::FeatureList::IsEnabled(
          device::kWebAuthnEnableAndroidCableAuthenticator) ||
      !state->device_supports_cable()) {
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
  }

  syncer::DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
  paask_info.tunnel_server_domain = device::cablev2::kTunnelServer.value();
  paask_info.contact_id = *state->sync_registration()->contact_id();
  const uint32_t pairing_id = device::cablev2::sync::IDNow();
  paask_info.id = pairing_id;

  std::array<uint8_t, device::cablev2::kPairingIDSize> pairing_id_bytes = {0};
  static_assert(sizeof(pairing_id) <= EXTENT(pairing_id_bytes), "");
  memcpy(pairing_id_bytes.data(), &pairing_id, sizeof(pairing_id));

  paask_info.secret = device::cablev2::Derive<EXTENT(paask_info.secret)>(
      state->secret(), pairing_id_bytes,
      device::cablev2::DerivedValueType::kPairedSecret);

  CHECK_EQ(paask_info.peer_public_key_x962.size(),
           EC_POINT_point2oct(EC_KEY_get0_group(state->identity_key()),
                              EC_KEY_get0_public_key(state->identity_key()),
                              POINT_CONVERSION_UNCOMPRESSED,
                              paask_info.peer_public_key_x962.data(),
                              paask_info.peer_public_key_x962.size(),
                              /*ctx=*/nullptr));

  return paask_info;
}

void SetPrefIfDifferent(PrefService* state,
                        const char* pref_name,
                        const std::string& value) {
  const std::string existing_value = state->GetString(pref_name);
  if (existing_value != value) {
    state->SetString(pref_name, value);
  }
}

}  // namespace

namespace internal {

std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> PaaskInfoFromCBOR(
    base::span<const uint8_t> cbor) {
  std::optional<cbor::Value> value = cbor::Reader::Read(cbor);
  if (!value || !value->is_map()) {
    return std::nullopt;
  }

  PreLinkInfo info;
  uint64_t pairing_id;
  std::array<uint8_t, 32> secret;
  std::array<uint8_t, 65> peer_public_key_x962;
  if (!device::cbor_extract::Extract<PreLinkInfo>(&info, kPreLinkInfoSteps,
                                                  value->GetMap()) ||
      info.pairing_id->size() != sizeof(pairing_id) ||
      info.secret->size() != secret.size() ||
      info.peer_public_key_x962->size() != peer_public_key_x962.size()) {
    return std::nullopt;
  }
  memcpy(&pairing_id, info.pairing_id->data(), sizeof(pairing_id));

  memcpy(secret.data(), info.secret->data(), secret.size());
  memcpy(peer_public_key_x962.data(), info.peer_public_key_x962->data(),
         peer_public_key_x962.size());

  syncer::DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
  paask_info.tunnel_server_domain = device::cablev2::kTunnelServer.value();
  paask_info.contact_id = std::move(*info.contact_id);
  if (pairing_id > std::numeric_limits<uint32_t>::max()) {
    return std::nullopt;
  }
  paask_info.id = static_cast<uint32_t>(pairing_id);
  paask_info.secret = secret;
  paask_info.peer_public_key_x962 = peer_public_key_x962;
  return paask_info;
}

std::vector<uint8_t> CBORFromPaaskInfo(
    const syncer::DeviceInfo::PhoneAsASecurityKeyInfo& paask_info) {
  cbor::Value::MapValue map;

  map.emplace(1, paask_info.contact_id);

  const uint64_t pairing_id = paask_info.id;
  uint8_t pairing_id_bytes[sizeof(pairing_id)];
  memcpy(pairing_id_bytes, &pairing_id, sizeof(pairing_id));
  map.emplace(2, std::vector<uint8_t>(std::begin(pairing_id_bytes),
                                      std::end(pairing_id_bytes)));

  map.emplace(3, paask_info.secret);

  map.emplace(4,
              std::vector<uint8_t>(std::begin(paask_info.peer_public_key_x962),
                                   std::end(paask_info.peer_public_key_x962)));

  return cbor::Writer::Write(cbor::Value(std::move(map))).value();
}

syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo CacheResult(
    syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo result,
    PrefService* state) {
  // kNoSupportString indicates that there is no support for PaaSK. It is
  // distinct from all base64-encoded values so is distinguishable from an
  // encoded `PhoneAsASecurityKeyInfo`.
  constexpr char kNoSupportString[] = ",";

  if (absl::get_if<syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NotReady>(
          &result)) {
    const std::string previous_result_serialized_b64 =
        state->GetString(kSerializedPaaskFieldsName);
    if (previous_result_serialized_b64 == kNoSupportString) {
      return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
    }

    std::string previous_result_serialized;
    if (previous_result_serialized_b64.empty() ||
        !base::Base64Decode(previous_result_serialized_b64,
                            &previous_result_serialized)) {
      return result;
    }

    std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> paask_info =
        internal::PaaskInfoFromCBOR(base::as_bytes(
            base::span<const char>(previous_result_serialized.begin(),
                                   previous_result_serialized.end())));
    if (!paask_info) {
      return result;
    }
    return *paask_info;
  } else if (auto* paask_info =
                 absl::get_if<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>(
                     &result)) {
    SetPrefIfDifferent(
        state, kSerializedPaaskFieldsName,
        base::Base64Encode(internal::CBORFromPaaskInfo(*paask_info)));
    return result;
  } else if (absl::get_if<
                 syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport>(
                 &result)) {
    SetPrefIfDifferent(state, kSerializedPaaskFieldsName, kNoSupportString);
    return result;
  }

  NOTREACHED();
}

}  // namespace internal

void RegisterForCloudMessages() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetRegistrationState()->Register();
}

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kRootSecretPrefName, std::string());
  registry->RegisterStringPref(kSerializedPaaskFieldsName, std::string());
  registry->RegisterStringPref(kWorkProfilePrefName, std::string());
}

syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
GetSyncDataIfRegistered() {
  return internal::CacheResult(GetSyncDataIfRegisteredInternal(),
                               g_browser_process->local_state());
}

}  // namespace authenticator
}  // namespace webauthn

using webauthn::authenticator::SystemInterface;

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

static void JNI_CableAuthenticatorModuleProvider_OnHaveLinkingInformation(
    JNIEnv* env,
    jlong system_interface_pointer,
    const base::android::JavaParamRef<jbyteArray>& cbor_java) {
  std::optional<std::vector<uint8_t>> optional_cbor;

  if (cbor_java) {
    std::vector<uint8_t> cbor;
    base::android::JavaByteArrayToByteVector(env, cbor_java, &cbor);
    optional_cbor = std::move(cbor);
  }

  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&SystemInterface::OnHavePlayServicesLinkingInformation,
                         base::Unretained(reinterpret_cast<SystemInterface*>(
                             static_cast<uintptr_t>(system_interface_pointer))),
                         std::move(optional_cbor)));
}

static void JNI_CableAuthenticatorModuleProvider_OnHaveWorkProfileResult(
    JNIEnv* env,
    jlong system_interface_pointer,
    jboolean in_work_profile) {
  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&SystemInterface::OnHaveWorkProfileResult,
                         base::Unretained(reinterpret_cast<SystemInterface*>(
                             static_cast<uintptr_t>(system_interface_pointer))),
                         in_work_profile));
}

static void JNI_PrivacySettingsFragment_RevokeAllLinkedDevices(JNIEnv* env) {
  // Invalidates the current cloud messaging (GCM) token and creates a new one.
  // This causes the tunnel server to reject connection attempts with a 410
  // (Gone) error. Since linking keys are derived from the root secret by using
  // the GCM token, this also invalidates all existing linking keys.
  webauthn::authenticator::GetRegistrationState()
      ->linking_registration()
      ->RotateContactID();
}
