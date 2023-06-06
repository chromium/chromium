// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/cable_module_android.h"

#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sys_byteorder.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
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
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    prelink_play_services_ =
        base::FeatureList::IsEnabled(device::kWebAuthnPrelinkPlayServices);

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
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &RegistrationState::GetCanDeviceSupportCableOnBackgroundSequence),
        base::BindOnce(&RegistrationState::OnDeviceSupportResult,
                       base::Unretained(this)));

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

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &RegistrationState::CalculateIdentityKeyOnBackgroundSequence,
            secret_),
        base::BindOnce(&RegistrationState::OnIdentityKeyReady,
                       base::Unretained(this)));
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
    return device_supports_cable_.has_value() && identity_key_ &&
           sync_registration_ != nullptr && sync_registration_->contact_id() &&
           have_play_services_data();
  }

  const EC_KEY* identity_key() const {
    DCHECK(identity_key_);
    return identity_key_.get();
  }

  bool device_supports_cable() const { return *device_supports_cable_; }
  bool prelink_play_services() const { return prelink_play_services_; }

  const absl::optional<std::vector<uint8_t>>& link_data_from_play_services()
      const {
    DCHECK(prelink_play_services_);
    DCHECK(have_link_data_from_play_services_);
    return link_data_from_play_services_;
  }

  void SignalSyncWhenReady() {
    if (sync_registration_ && !sync_registration_->contact_id()) {
      sync_registration_->PrepareContactID();
    }
    if (!have_play_services_data() && !play_services_query_pending_) {
      QueryPlayServices();
    }
    signal_sync_when_ready_ = true;
  }

  void OnHavePlayServicesLinkingInformation(
      absl::optional<std::vector<uint8_t>> cbor) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(play_services_query_pending_);

    play_services_query_pending_ = false;
    link_data_from_play_services_ = std::move(cbor);
    have_link_data_from_play_services_ = true;
    link_data_from_play_services_timeticks_ = base::TimeTicks::Now();
    MaybeSignalSync();
  }

 private:
  bool have_play_services_data() const {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    // If querying Play Services is disabled then it's always "ready".
    if (!prelink_play_services_) {
      return true;
    }
    // If there's no result, then we're not ready.
    if (!have_link_data_from_play_services_) {
      return false;
    }
    // If there's a query already pending then the result must be stale and
    // there's nothing more to do here.
    if (play_services_query_pending_) {
      return false;
    }
    const base::TimeDelta staleness =
        base::TimeTicks::Now() - link_data_from_play_services_timeticks_;
    return staleness < base::Hours(12);
  }

  void QueryPlayServices() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!play_services_query_pending_);

    play_services_query_pending_ = true;
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&RegistrationState::
                           GetPrelinkFromPlayServicesOnBackgroundSequence));
  }

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
        !pending_event_->contact_id) {
      // This GCM message is from a QR-linked peer so it needs the contact ID
      // to be processed.
      pending_event_->contact_id = linking_registration_->contact_id();

      if (!pending_event_->contact_id) {
        // The contact ID isn't ready yet. Wait until it is.
        linking_registration_->PrepareContactID();
        return;
      }
    }

    std::unique_ptr<Registration::Event> event(std::move(pending_event_));
    if (event->source == Registration::Type::SYNC) {
      // If this is from a synced peer then we limit how old the keys can be.
      // Clank will update its device information once per day (when launched)
      // and we piggyback on that to transmit fresh keys. Therefore syncing
      // peers should have reasonably recent information.
      uint64_t id;
      static_assert(EXTENT(event->pairing_id) == sizeof(id), "");
      memcpy(&id, event->pairing_id.data(), sizeof(id));

      // A maximum age is enforced for sync secrets so that any leak of
      // information isn't valid forever. The desktop ignores DeviceInfo
      // records with information that is too old so this should never happen
      // with honest clients.
      if (id > std::numeric_limits<uint32_t>::max() ||
          device::cablev2::sync::IDIsMoreThanNPeriodsOld(
              static_cast<uint32_t>(id),
              device::cablev2::kMaxSyncInfoDaysForProducer)) {
        LOG(ERROR) << "Pairing ID " << id << " is too old. Dropping.";
        return;
      }
    }

    const absl::optional<std::vector<uint8_t>> serialized(event->Serialize());
    if (!serialized) {
      return;
    }

    JNIEnv* const env = base::android::AttachCurrentThread();
    Java_CableAuthenticatorModuleProvider_onCloudMessage(
        env, base::android::ToJavaByteArray(env, *serialized),
        event->request_type == device::FidoRequestType::kMakeCredential);
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

  static bool GetCanDeviceSupportCableOnBackgroundSequence() {
    // This runs on a worker thread because this Java function can take a
    // little while and it shouldn't block the UI thread.
    return Java_CableAuthenticatorModuleProvider_canDeviceSupportCable(
        base::android::AttachCurrentThread());
  }

  static void GetPrelinkFromPlayServicesOnBackgroundSequence() {
    // This runs on a worker thread because this Java function can take a
    // little while and it shouldn't block the UI thread.
    Java_CableAuthenticatorModuleProvider_getLinkingInformation(
        base::android::AttachCurrentThread());
  }

  // OnCanDeviceSupportCable is run with the result of `TestDeviceSupport`.
  void OnDeviceSupportResult(bool result) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    device_supports_cable_ = result;
    MaybeSignalSync();
  }

  static bssl::UniquePtr<EC_KEY> CalculateIdentityKeyOnBackgroundSequence(
      std::array<uint8_t, 32> secret) {
    // This runs on a worker thread because the scalar multiplication takes a
    // few milliseconds on slower devices.
    return device::cablev2::IdentityKey(secret);
  }

  // OnIdentityKeyReady is run with the result of `CalculateIdentityKey`.
  void OnIdentityKeyReady(bssl::UniquePtr<EC_KEY> identity_key) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    identity_key_ = std::move(identity_key);
    MaybeSignalSync();
  }

  std::unique_ptr<Registration> linking_registration_;
  std::unique_ptr<Registration> sync_registration_;
  std::array<uint8_t, 32> secret_;
  // identity_key_ is a public/private P-256 key that is calculated from
  // `secret_`. It's cached because it takes some time to compute.
  bssl::UniquePtr<EC_KEY> identity_key_;
  std::unique_ptr<Registration::Event> pending_event_;
  // device_supports_cable_ caches the result of a Java function that checks
  // some prerequisites: that the device has Bluetooth and a screenlock. If
  // this value is |nullopt| then its value has not yet been determined.
  //
  // The presence of a screen lock could change but, because of this caching,
  // Clank won't notice in this context until the process restarts. Users can
  // always use a QR code if pre-linking hasn't worked by the time they need
  // it.
  absl::optional<bool> device_supports_cable_;
  // link_data_from_play_services_ contains the response from Play Services, as
  // CBOR-encoded linking information, or `nullopt` if the call was
  // unsuccessful. This field is only meaningful if
  // `have_link_data_from_play_services_` is true.
  absl::optional<std::vector<uint8_t>> link_data_from_play_services_;
  // have_link_data_from_play_services_ is true if any call to Play Services has
  // ever completed, successful or not.
  bool have_link_data_from_play_services_ = false;
  // link_data_from_play_services_timeticks_ contains the timestamp when
  // `link_data_from_play_services_` was set.
  base::TimeTicks link_data_from_play_services_timeticks_;
  // play_services_query_pending_ is true if a request to Play Services is
  // currently outstanding.
  bool play_services_query_pending_ = false;
  bool signal_sync_when_ready_ = false;
  // prelink_play_services_ records the value of the feature flag
  // `kWebAuthnPrelinkPlayServices`. It's recorded here because its value could
  // change at run-time, but this code doesn't handle that.
  bool prelink_play_services_ = false;
};

RegistrationState* GetRegistrationState() {
  static base::NoDestructor<RegistrationState> state;
  return state.get();
}

using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::Map;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;

// PreLinkInfo reflects the linking information provided by Play Services.
struct PreLinkInfo {
  // All fields below are not a raw_ptr<T> because cbor_extract.cc would
  // cast the raw_ptr<T> to a void*, skipping an AddRef() call and causing a
  // ref-counting mismatch.
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

}  // namespace

namespace internal {

absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> PaaskInfoFromCBOR(
    base::span<const uint8_t> cbor) {
  absl::optional<cbor::Value> value = cbor::Reader::Read(cbor);
  if (!value || !value->is_map()) {
    return absl::nullopt;
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
    return absl::nullopt;
  }
  memcpy(&pairing_id, info.pairing_id->data(), sizeof(pairing_id));

  memcpy(secret.data(), info.secret->data(), secret.size());
  memcpy(peer_public_key_x962.data(), info.peer_public_key_x962->data(),
         peer_public_key_x962.size());

  syncer::DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
  paask_info.tunnel_server_domain = device::cablev2::kTunnelServer.value();
  paask_info.contact_id = std::move(*info.contact_id);
  if (pairing_id > std::numeric_limits<uint32_t>::max()) {
    return absl::nullopt;
  }
  paask_info.id = static_cast<uint32_t>(pairing_id);
  paask_info.secret = secret;
  paask_info.peer_public_key_x962 = peer_public_key_x962;
  return paask_info;
}

}  // namespace internal

void RegisterForCloudMessages() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetRegistrationState()->Register();
}

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kRootSecretPrefName, std::string());
}

absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>
GetSyncDataIfRegistered() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  RegistrationState* state = GetRegistrationState();
  if (!state->have_data_for_sync()) {
    // Not yet ready to provide sync data. When the data is ready,
    // |state| will signal to Sync that something changed and this
    // function will be called again.
    state->SignalSyncWhenReady();
    return absl::nullopt;
  }

  if (state->prelink_play_services() && state->link_data_from_play_services()) {
    absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> paask_info =
        internal::PaaskInfoFromCBOR(*state->link_data_from_play_services());
    if (paask_info) {
      return *paask_info;
    } else {
      LOG(ERROR)
          << "Failed to parse PaaSK prelink information from Play Services";
    }
  }

  if (!state->device_supports_cable()) {
    return absl::nullopt;
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

static void OnHavePlayServicesLinkingInformation(
    absl::optional<std::vector<uint8_t>> cbor) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  webauthn::authenticator::GetRegistrationState()
      ->OnHavePlayServicesLinkingInformation(std::move(cbor));
}

static void JNI_CableAuthenticatorModuleProvider_OnHaveLinkingInformation(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& cbor_java) {
  absl::optional<std::vector<uint8_t>> optional_cbor;

  if (cbor_java) {
    std::vector<uint8_t> cbor;
    base::android::JavaByteArrayToByteVector(env, cbor_java, &cbor);
    optional_cbor = std::move(cbor);
  }

  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&OnHavePlayServicesLinkingInformation,
                                std::move(optional_cbor)));
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
