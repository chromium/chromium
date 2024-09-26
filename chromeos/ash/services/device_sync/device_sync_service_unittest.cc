// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_enroller.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_manager_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_enrollment_manager_impl.h"
#include "chromeos/ash/services/device_sync/device_sync_impl.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_device_manager.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_device_notifier.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_enrollment_manager.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_feature_status_setter.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_v2_device_manager.h"
#include "chromeos/ash/services/device_sync/fake_device_sync_observer.h"
#include "chromeos/ash/services/device_sync/fake_remote_device_provider.h"
#include "chromeos/ash/services/device_sync/fake_software_feature_manager.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_prefs.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_client_app_metadata_provider.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_gcm_device_info_provider.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "chromeos/ash/services/device_sync/remote_device_provider_impl.h"
#include "chromeos/ash/services/device_sync/software_feature_manager_impl.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kTestEmail[] = "example@gmail.com";
const char kTestGcmDeviceInfoLongDeviceId[] = "longDeviceId";
const char kTestCryptAuthGCMRegistrationId[] =
    "aValid:cryptAuth-RegistrationId";
const char kTestDeprecatedCryptAuthGCMRegistrationId[] =
    "inValid-cryptAuthRegistrationId";
const char kLocalDevicePublicKey[] = "localDevicePublicKey";
const size_t kNumTestDevices = 5u;

const cryptauth::GcmDeviceInfo& GetTestGcmDeviceInfo() {
  static const base::NoDestructor<cryptauth::GcmDeviceInfo> gcm_device_info([] {
    cryptauth::GcmDeviceInfo gcm_device_info;
    gcm_device_info.set_long_device_id(kTestGcmDeviceInfoLongDeviceId);
    return gcm_device_info;
  }());

  return *gcm_device_info;
}

multidevice::RemoteDeviceList GenerateTestRemoteDevices() {
  multidevice::RemoteDeviceList devices =
      multidevice::CreateRemoteDeviceListForTest(kNumTestDevices);

  // One of the synced devices refers to the current (i.e., local) device.
  // Arbitrarily choose the 0th device as the local one and set its public key
  // accordingly.
  devices[0].public_key = kLocalDevicePublicKey;

  return devices;
}

std::vector<cryptauth::ExternalDeviceInfo> GenerateTestExternalDeviceInfos(
    const multidevice::RemoteDeviceList& remote_devices) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos;

  for (const auto& remote_device : remote_devices) {
    cryptauth::ExternalDeviceInfo info;
    info.set_public_key(remote_device.public_key);
    device_infos.push_back(info);
  }

  return device_infos;
}

std::vector<cryptauth::IneligibleDevice> GenerateTestIneligibleDevices(
    const std::vector<cryptauth::ExternalDeviceInfo>& device_infos) {
  std::vector<cryptauth::IneligibleDevice> ineligible_devices;

  for (const auto& device_info : device_infos) {
    cryptauth::IneligibleDevice device;
    device.mutable_device()->CopyFrom(device_info);
    ineligible_devices.push_back(device);
  }

  return ineligible_devices;
}

// Delegate which invokes the Closure provided to its constructor when a
// delegate function is invoked.
class FakeSoftwareFeatureManagerDelegate
    : public FakeSoftwareFeatureManager::Delegate {
 public:
  explicit FakeSoftwareFeatureManagerDelegate(
      base::RepeatingClosure on_delegate_call_closure)
      : on_delegate_call_closure_(on_delegate_call_closure) {}

  ~FakeSoftwareFeatureManagerDelegate() override = default;

  // FakeSoftwareFeatureManager::Delegate:
  void OnSetSoftwareFeatureStateCalled() override {
    on_delegate_call_closure_.Run();
  }
  void OnSetFeatureStatusCalled() override { on_delegate_call_closure_.Run(); }
  void OnFindEligibleDevicesCalled() override {
    on_delegate_call_closure_.Run();
  }

 private:
  base::RepeatingClosure on_delegate_call_closure_;
};

// Delegate which invokes the Closure provided to its constructor when a
// delegate function is invoked.
class FakeCryptAuthFeatureStatusSetterDelegate
    : public FakeCryptAuthFeatureStatusSetter::Delegate {
 public:
  explicit FakeCryptAuthFeatureStatusSetterDelegate(
      base::RepeatingClosure on_delegate_call_closure)
      : on_delegate_call_closure_(on_delegate_call_closure) {}

  ~FakeCryptAuthFeatureStatusSetterDelegate() override = default;

  // FakeCryptAuthFeatureStatusSetter::Delegate:
  void OnSetFeatureStatusCalled() override { on_delegate_call_closure_.Run(); }

 private:
  base::RepeatingClosure on_delegate_call_closure_;
};

// Delegate which invokes the Closure provided to its constructor when a
// delegate function is invoked.
class FakeCryptAuthDeviceNotifierDelegate
    : public FakeCryptAuthDeviceNotifier::Delegate {
 public:
  explicit FakeCryptAuthDeviceNotifierDelegate(
      base::RepeatingClosure on_delegate_call_closure)
      : on_delegate_call_closure_(on_delegate_call_closure) {}

  ~FakeCryptAuthDeviceNotifierDelegate() override = default;

  // FakeCryptAuthDeviceNotifier::Delegate:
  void OnNotifyDevicesCalled() override { on_delegate_call_closure_.Run(); }

 private:
  base::RepeatingClosure on_delegate_call_closure_;
};

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;
  MOCK_METHOD(instance_id::InstanceID*,
              GetInstanceID,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void, RemoveInstanceID, (const std::string& app_id), (override));
  MOCK_METHOD(bool,
              ExistsInstanceID,
              (const std::string& app_id),
              (const override));
};

class FakeCryptAuthGCMManagerFactory : public CryptAuthGCMManagerImpl::Factory {
 public:
  FakeCryptAuthGCMManagerFactory(
      gcm::FakeGCMDriver* fake_gcm_driver,
      instance_id::InstanceIDDriver* fake_instance_id_driver,
      TestingPrefServiceSimple* test_pref_service)
      : fake_gcm_driver_(fake_gcm_driver),
        fake_instance_id_driver_(fake_instance_id_driver),
        test_pref_service_(test_pref_service) {}

  ~FakeCryptAuthGCMManagerFactory() override = default;

  FakeCryptAuthGCMManager* instance() { return instance_; }

  void SetInitialRegistrationId(const std::string& registration_id) {
    initial_registration_id_ = registration_id;
  }

 private:
  // CryptAuthGCMManagerImpl::Factory:
  std::unique_ptr<CryptAuthGCMManager> CreateInstance(
      gcm::GCMDriver* fake_gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver,
      PrefService* pref_service) override {
    EXPECT_EQ(fake_gcm_driver_, fake_gcm_driver);
    EXPECT_EQ(fake_instance_id_driver_, instance_id_driver);
    EXPECT_EQ(test_pref_service_, pref_service);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance =
        std::make_unique<FakeCryptAuthGCMManager>(initial_registration_id_);
    instance_ = instance.get();

    return instance;
  }

  raw_ptr<gcm::FakeGCMDriver, DanglingUntriaged> fake_gcm_driver_;
  raw_ptr<instance_id::InstanceIDDriver, DanglingUntriaged>
      fake_instance_id_driver_;
  raw_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::string initial_registration_id_;
  raw_ptr<FakeCryptAuthGCMManager, DanglingUntriaged> instance_ = nullptr;
};

class FakeCryptAuthDeviceRegistry : public CryptAuthDeviceRegistry {
 public:
  FakeCryptAuthDeviceRegistry() = default;
  ~FakeCryptAuthDeviceRegistry() override = default;

 private:
  // CryptAuthDeviceRegistry:
  void OnDeviceRegistryUpdated() override {}
};

class FakeCryptAuthDeviceRegistryFactory
    : public CryptAuthDeviceRegistryImpl::Factory {
 public:
  explicit FakeCryptAuthDeviceRegistryFactory(
      TestingPrefServiceSimple* test_pref_service)
      : test_pref_service_(test_pref_service) {}

  ~FakeCryptAuthDeviceRegistryFactory() override = default;

  FakeCryptAuthDeviceRegistry* instance() { return instance_; }

 private:
  // CryptAuthDeviceRegistryImpl::Factory:
  std::unique_ptr<CryptAuthDeviceRegistry> CreateInstance(
      PrefService* pref_service) override {
    EXPECT_TRUE(features::ShouldUseV2DeviceSync());
    EXPECT_EQ(test_pref_service_, pref_service);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);
    auto instance = std::make_unique<FakeCryptAuthDeviceRegistry>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<TestingPrefServiceSimple> test_pref_service_;
  raw_ptr<FakeCryptAuthDeviceRegistry, DanglingUntriaged> instance_ = nullptr;
};

class FakeCryptAuthKeyRegistry : public CryptAuthKeyRegistry {
 public:
  FakeCryptAuthKeyRegistry() = default;
  ~FakeCryptAuthKeyRegistry() override = default;

 private:
  // CryptAuthKeyRegistry:
  void OnKeyRegistryUpdated() override {}
};

class FakeCryptAuthKeyRegistryFactory
    : public CryptAuthKeyRegistryImpl::Factory {
 public:
  explicit FakeCryptAuthKeyRegistryFactory(
      TestingPrefServiceSimple* test_pref_service)
      : test_pref_service_(test_pref_service) {}

  ~FakeCryptAuthKeyRegistryFactory() override = default;

  FakeCryptAuthKeyRegistry* instance() { return instance_; }

 private:
  // CryptAuthKeyRegistryImpl::Factory:
  std::unique_ptr<CryptAuthKeyRegistry> CreateInstance(
      PrefService* pref_service) override {
    EXPECT_EQ(test_pref_service_, pref_service);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);
    auto instance = std::make_unique<FakeCryptAuthKeyRegistry>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<TestingPrefServiceSimple> test_pref_service_;
  raw_ptr<FakeCryptAuthKeyRegistry, DanglingUntriaged> instance_ = nullptr;
};

class FakeCryptAuthSchedulerFactory : public CryptAuthSchedulerImpl::Factory {
 public:
  explicit FakeCryptAuthSchedulerFactory(
      TestingPrefServiceSimple* test_pref_service)
      : test_pref_service_(test_pref_service) {}

  ~FakeCryptAuthSchedulerFactory() override = default;

  FakeCryptAuthScheduler* instance() { return instance_; }

 private:
  // CryptAuthSchedulerImpl::Factory:
  std::unique_ptr<CryptAuthScheduler> CreateInstance(
      PrefService* pref_service,
      NetworkStateHandler* network_state_handler,
      base::Clock* clock,
      std::unique_ptr<base::OneShotTimer> enrollment_timer,
      std::unique_ptr<base::OneShotTimer> device_sync_timer) override {
    EXPECT_EQ(test_pref_service_, pref_service);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<FakeCryptAuthScheduler>();
    instance_ = instance.get();

    return instance;
  }

  raw_ptr<TestingPrefServiceSimple> test_pref_service_;
  raw_ptr<FakeCryptAuthScheduler, DanglingUntriaged> instance_ = nullptr;
};

class FakeCryptAuthV2DeviceManagerFactory
    : public CryptAuthV2DeviceManagerImpl::Factory {
 public:
  FakeCryptAuthV2DeviceManagerFactory(
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      FakeCryptAuthDeviceRegistryFactory* fake_device_registry_factory,
      FakeCryptAuthKeyRegistryFactory* fake_key_registry_factory,
      FakeCryptAuthGCMManagerFactory* fake_gcm_manager_factory,
      FakeCryptAuthSchedulerFactory* fake_scheduler_factory,
      TestingPrefServiceSimple* test_pref_service)
      : client_app_metadata_(client_app_metadata),
        fake_device_registry_factory_(fake_device_registry_factory),
        fake_key_registry_factory_(fake_key_registry_factory),
        fake_gcm_manager_factory_(fake_gcm_manager_factory),
        fake_scheduler_factory_(fake_scheduler_factory),
        test_pref_service_(test_pref_service) {}

  ~FakeCryptAuthV2DeviceManagerFactory() override = default;

  FakeCryptAuthV2DeviceManager* instance() { return instance_; }

 private:
  // CryptAuthV2DeviceManagerImpl::Factory:
  std::unique_ptr<CryptAuthV2DeviceManager> CreateInstance(
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      CryptAuthDeviceRegistry* device_registry,
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      CryptAuthGCMManager* gcm_manager,
      CryptAuthScheduler* scheduler,
      PrefService* pref_service,
      AttestationCertificatesSyncer::GetAttestationCertificatesFunction
          get_attestation_certificates_function) override {
    EXPECT_TRUE(features::ShouldUseV2DeviceSync());
    EXPECT_EQ(client_app_metadata_.SerializeAsString(),
              client_app_metadata.SerializeAsString());
    EXPECT_EQ(fake_device_registry_factory_->instance(), device_registry);
    EXPECT_EQ(fake_key_registry_factory_->instance(), key_registry);
    EXPECT_EQ(fake_gcm_manager_factory_->instance(), gcm_manager);
    EXPECT_EQ(fake_scheduler_factory_->instance(), scheduler);
    EXPECT_EQ(test_pref_service_, pref_service);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<FakeCryptAuthV2DeviceManager>();
    instance_ = instance.get();

    return instance;
  }

  cryptauthv2::ClientAppMetadata client_app_metadata_;
  raw_ptr<FakeCryptAuthDeviceRegistryFactory> fake_device_registry_factory_ =
      nullptr;
  raw_ptr<FakeCryptAuthKeyRegistryFactory> fake_key_registry_factory_ = nullptr;
  raw_ptr<FakeCryptAuthGCMManagerFactory> fake_gcm_manager_factory_ = nullptr;
  raw_ptr<FakeCryptAuthSchedulerFactory> fake_scheduler_factory_ = nullptr;
  raw_ptr<TestingPrefServiceSimple> test_pref_service_ = nullptr;
  raw_ptr<FakeCryptAuthV2DeviceManager, DanglingUntriaged> instance_ = nullptr;
};

class FakeCryptAuthV2EnrollmentManagerFactory
    : public CryptAuthV2EnrollmentManagerImpl::Factory {
 public:
  FakeCryptAuthV2EnrollmentManagerFactory(
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      FakeCryptAuthKeyRegistryFactory* fake_cryptauth_key_registry_factory,
      FakeCryptAuthGCMManagerFactory* fake_cryptauth_gcm_manager_factory,
      FakeCryptAuthSchedulerFactory* fake_cryptauth_scheduler_factory,
      TestingPrefServiceSimple* test_pref_service,
      base::SimpleTestClock* simple_test_clock)
      : client_app_metadata_(client_app_metadata),
        fake_cryptauth_key_registry_factory_(
            fake_cryptauth_key_registry_factory),
        fake_cryptauth_gcm_manager_factory_(fake_cryptauth_gcm_manager_factory),
        fake_cryptauth_scheduler_factory_(fake_cryptauth_scheduler_factory),
        test_pref_service_(test_pref_service),
        simple_test_clock_(simple_test_clock) {}

  ~FakeCryptAuthV2EnrollmentManagerFactory() override = default;

  void set_device_already_enrolled_in_cryptauth(
      bool device_already_enrolled_in_cryptauth) {
    device_already_enrolled_in_cryptauth_ =
        device_already_enrolled_in_cryptauth;
  }

  FakeCryptAuthEnrollmentManager* instance() { return instance_; }

  // CryptAuthV2EnrollmentManagerImpl::Factory:
  std::unique_ptr<CryptAuthEnrollmentManager> CreateInstance(
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      CryptAuthGCMManager* gcm_manager,
      CryptAuthScheduler* scheduler,
      PrefService* pref_service,
      base::Clock* clock) override {
    EXPECT_EQ(client_app_metadata_.SerializeAsString(),
              client_app_metadata.SerializeAsString());
    EXPECT_EQ(fake_cryptauth_key_registry_factory_->instance(), key_registry);
    EXPECT_EQ(fake_cryptauth_gcm_manager_factory_->instance(), gcm_manager);
    EXPECT_EQ(fake_cryptauth_scheduler_factory_->instance(), scheduler);
    EXPECT_EQ(test_pref_service_, pref_service);
    EXPECT_EQ(simple_test_clock_, clock);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<FakeCryptAuthEnrollmentManager>();
    instance->set_user_public_key(kLocalDevicePublicKey);
    instance->set_is_enrollment_valid(device_already_enrolled_in_cryptauth_);
    instance_ = instance.get();
    LOG(INFO) << " made enrollment manager 2";
    return instance;
  }

 private:
  cryptauthv2::ClientAppMetadata client_app_metadata_;
  raw_ptr<FakeCryptAuthKeyRegistryFactory> fake_cryptauth_key_registry_factory_;
  raw_ptr<FakeCryptAuthGCMManagerFactory> fake_cryptauth_gcm_manager_factory_;
  raw_ptr<FakeCryptAuthSchedulerFactory> fake_cryptauth_scheduler_factory_;
  raw_ptr<TestingPrefServiceSimple> test_pref_service_;
  raw_ptr<base::SimpleTestClock> simple_test_clock_;
  bool device_already_enrolled_in_cryptauth_ = false;
  raw_ptr<FakeCryptAuthEnrollmentManager, DanglingUntriaged> instance_ =
      nullptr;
};

class FakeRemoteDeviceProviderFactory
    : public RemoteDeviceProviderImpl::Factory {
 public:
  FakeRemoteDeviceProviderFactory(
      const multidevice::RemoteDeviceList& initial_devices,
      signin::IdentityManager* identity_manager,
      FakeCryptAuthV2DeviceManagerFactory*
          fake_cryptauth_v2_device_manager_factory,
      FakeCryptAuthV2EnrollmentManagerFactory*
          fake_cryptauth_v2_enrollment_manager_factory)
      : initial_devices_(initial_devices),
        identity_manager_(identity_manager),
        fake_cryptauth_v2_device_manager_factory_(
            fake_cryptauth_v2_device_manager_factory),
        fake_cryptauth_v2_enrollment_manager_factory_(
            fake_cryptauth_v2_enrollment_manager_factory) {}

  ~FakeRemoteDeviceProviderFactory() override = default;

  FakeRemoteDeviceProvider* instance() { return instance_; }

  // RemoteDeviceProviderImpl::Factory:
  std::unique_ptr<RemoteDeviceProvider> CreateInstance(
      CryptAuthV2DeviceManager* v2_device_manager,
      const std::string& user_email,
      const std::string& user_private_key) override {
    EXPECT_EQ(fake_cryptauth_v2_device_manager_factory_->instance(),
              v2_device_manager);
    EXPECT_EQ(
        identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email,
        user_email);
    EXPECT_EQ(fake_cryptauth_v2_enrollment_manager_factory_->instance()
                  ->GetUserPrivateKey(),
              user_private_key);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<FakeRemoteDeviceProvider>();
    instance->set_synced_remote_devices(*initial_devices_);
    instance_ = instance.get();

    return instance;
  }

 private:
  const raw_ref<const multidevice::RemoteDeviceList> initial_devices_;

  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  raw_ptr<FakeCryptAuthV2DeviceManagerFactory>
      fake_cryptauth_v2_device_manager_factory_;
  raw_ptr<FakeCryptAuthV2EnrollmentManagerFactory>
      fake_cryptauth_v2_enrollment_manager_factory_;

  raw_ptr<FakeRemoteDeviceProvider, DanglingUntriaged> instance_ = nullptr;
};

class FakeSoftwareFeatureManagerFactory
    : public SoftwareFeatureManagerImpl::Factory {
 public:
  FakeSoftwareFeatureManagerFactory() = default;
  ~FakeSoftwareFeatureManagerFactory() override = default;

  FakeSoftwareFeatureManager* instance() { return instance_; }

  // SoftwareFeatureManagerImpl::Factory:
  std::unique_ptr<SoftwareFeatureManager> CreateInstance(
      CryptAuthClientFactory* cryptauth_client_factory,
      CryptAuthFeatureStatusSetter* feature_status_setter) override {
    EXPECT_TRUE(features::ShouldUseV1DeviceSync());

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<FakeSoftwareFeatureManager>();
    instance_ = instance.get();

    return instance;
  }

 private:
  raw_ptr<FakeSoftwareFeatureManager, DanglingUntriaged> instance_ = nullptr;
};

}  // namespace

// TODO(jamescook): Rename to DeviceSyncImplTest because it's actually testing
// the DeviceSync implementation.
class DeviceSyncServiceTest : public ::testing::Test {
 public:
  class FakeDeviceSyncImplFactory : public DeviceSyncImpl::Factory {
   public:
    FakeDeviceSyncImplFactory(
        std::unique_ptr<base::MockOneShotTimer> mock_timer,
        base::SimpleTestClock* simple_test_clock)
        : mock_timer_(std::move(mock_timer)),
          simple_test_clock_(simple_test_clock) {}

    ~FakeDeviceSyncImplFactory() override = default;

    // DeviceSyncImpl::Factory:
    std::unique_ptr<DeviceSyncBase> CreateInstance(
        signin::IdentityManager* identity_manager,
        gcm::GCMDriver* gcm_driver,
        instance_id::InstanceIDDriver* instance_id_driver,
        PrefService* profile_prefs,
        const GcmDeviceInfoProvider* gcm_device_info_provider,
        ClientAppMetadataProvider* client_app_metadata_provider,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::unique_ptr<base::OneShotTimer> timer,
        AttestationCertificatesSyncer::GetAttestationCertificatesFunction
            get_attestation_certificates_function) override {
      return base::WrapUnique(new DeviceSyncImpl(
          identity_manager, gcm_driver, instance_id_driver, profile_prefs,
          gcm_device_info_provider, client_app_metadata_provider,
          std::move(url_loader_factory), simple_test_clock_,
          std::move(mock_timer_), get_attestation_certificates_function));
    }

   private:
    std::unique_ptr<base::MockOneShotTimer> mock_timer_;
    raw_ptr<base::SimpleTestClock> simple_test_clock_;
  };

  DeviceSyncServiceTest()
      : test_devices_(GenerateTestRemoteDevices()),
        test_device_infos_(GenerateTestExternalDeviceInfos(test_devices_)),
        test_ineligible_devices_(
            GenerateTestIneligibleDevices(test_device_infos_)) {}

  DeviceSyncServiceTest(const DeviceSyncServiceTest&) = delete;
  DeviceSyncServiceTest& operator=(const DeviceSyncServiceTest&) = delete;

  ~DeviceSyncServiceTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    base::RunLoop().RunUntilIdle();

    fake_gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();

    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterProfilePrefs(test_pref_service_->registry());

    simple_test_clock_ = std::make_unique<base::SimpleTestClock>();

    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();

    fake_cryptauth_gcm_manager_factory_ =
        std::make_unique<FakeCryptAuthGCMManagerFactory>(
            fake_gcm_driver_.get(), &fake_instance_id_driver_,
            test_pref_service_.get());
    CryptAuthGCMManagerImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_gcm_manager_factory_.get());

    // ---------- Begin: Only used for v2 Enrollment ----------
    fake_client_app_metadata_provider_ =
        std::make_unique<FakeClientAppMetadataProvider>();

    fake_cryptauth_key_registry_factory_ =
        std::make_unique<FakeCryptAuthKeyRegistryFactory>(
            test_pref_service_.get());
    CryptAuthKeyRegistryImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_key_registry_factory_.get());

    fake_cryptauth_scheduler_factory_ =
        std::make_unique<FakeCryptAuthSchedulerFactory>(
            test_pref_service_.get());
    CryptAuthSchedulerImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_scheduler_factory_.get());

    fake_cryptauth_v2_enrollment_manager_factory_ =
        std::make_unique<FakeCryptAuthV2EnrollmentManagerFactory>(
            cryptauthv2::GetClientAppMetadataForTest(),
            fake_cryptauth_key_registry_factory_.get(),
            fake_cryptauth_gcm_manager_factory_.get(),
            fake_cryptauth_scheduler_factory_.get(), test_pref_service_.get(),
            simple_test_clock_.get());
    CryptAuthV2EnrollmentManagerImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_v2_enrollment_manager_factory_.get());
    // ---------- End: Only used for v2 Enrollment ----------

    // ---------- Begin: Only used for v2 DeviceSync ----------
    fake_cryptauth_device_registry_factory_ =
        std::make_unique<FakeCryptAuthDeviceRegistryFactory>(
            test_pref_service_.get());
    CryptAuthDeviceRegistryImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_device_registry_factory_.get());

    fake_cryptauth_v2_device_manager_factory_ =
        std::make_unique<FakeCryptAuthV2DeviceManagerFactory>(
            cryptauthv2::GetClientAppMetadataForTest(),
            fake_cryptauth_device_registry_factory_.get(),
            fake_cryptauth_key_registry_factory_.get(),
            fake_cryptauth_gcm_manager_factory_.get(),
            fake_cryptauth_scheduler_factory_.get(), test_pref_service_.get());
    CryptAuthV2DeviceManagerImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_v2_device_manager_factory_.get());

    fake_device_notifier_factory_ =
        std::make_unique<FakeCryptAuthDeviceNotifierFactory>();
    CryptAuthDeviceNotifierImpl::Factory::SetFactoryForTesting(
        fake_device_notifier_factory_.get());

    fake_feature_status_setter_factory_ =
        std::make_unique<FakeCryptAuthFeatureStatusSetterFactory>();
    CryptAuthFeatureStatusSetterImpl::Factory::SetFactoryForTesting(
        fake_feature_status_setter_factory_.get());
    // ---------- End: Only used for v2 DeviceSync ----------

    fake_remote_device_provider_factory_ =
        std::make_unique<FakeRemoteDeviceProviderFactory>(
            test_devices_, identity_test_environment_->identity_manager(),
            fake_cryptauth_v2_device_manager_factory_.get(),
            fake_cryptauth_v2_enrollment_manager_factory_.get());
    RemoteDeviceProviderImpl::Factory::SetFactoryForTesting(
        fake_remote_device_provider_factory_.get());

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    fake_device_sync_impl_factory_ =
        std::make_unique<FakeDeviceSyncImplFactory>(std::move(mock_timer),
                                                    simple_test_clock_.get());
    DeviceSyncImpl::Factory::SetCustomFactory(
        fake_device_sync_impl_factory_.get());

    fake_gcm_device_info_provider_ =
        std::make_unique<FakeGcmDeviceInfoProvider>(GetTestGcmDeviceInfo());
  }

  void TearDown() override {
    CryptAuthGCMManagerImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthDeviceManagerImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthKeyRegistryImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthV2EnrollmentManagerImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthEnrollmentManagerImpl::Factory::SetFactoryForTesting(nullptr);
    RemoteDeviceProviderImpl::Factory::SetFactoryForTesting(nullptr);
    SoftwareFeatureManagerImpl::Factory::SetFactoryForTesting(nullptr);
    DeviceSyncImpl::Factory::SetCustomFactory(nullptr);

    network_handler_test_helper_.reset();
  }

  // Creates and initializes |device_sync_|. Done here instead of in SetUp()
  // so we can control whether or not the primary account is available at
  // construction time.
  void InitializeDeviceSync(bool device_already_enrolled_in_cryptauth) {
    device_already_enrolled_in_cryptauth_ =
        device_already_enrolled_in_cryptauth;

    fake_cryptauth_v2_enrollment_manager_factory_
        ->set_device_already_enrolled_in_cryptauth(
            device_already_enrolled_in_cryptauth);

    auto shared_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));

    device_sync_ = DeviceSyncImpl::Factory::Create(
        identity_test_environment_->identity_manager(), fake_gcm_driver_.get(),
        &fake_instance_id_driver_, test_pref_service_.get(),
        fake_gcm_device_info_provider_.get(),
        fake_client_app_metadata_provider_.get(), shared_url_loader_factory,
        std::make_unique<base::OneShotTimer>(),
        base::BindRepeating(
            [](AttestationCertificatesSyncer::NotifyCallback notifyCallback,
               const std::string&) {}));

    fake_device_sync_observer_ = std::make_unique<FakeDeviceSyncObserver>();

    // Set |fake_device_sync_observer_|.
    CallAddObserver();
  }

  void MakePrimaryAccountAvailable() {
    identity_test_environment_->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
  }

  void SetInitialRegistrationId(const std::string& registration_id) {
    fake_cryptauth_gcm_manager_factory_->SetInitialRegistrationId(
        registration_id);
  }

  void SucceedGcmRegistration() {
    FakeCryptAuthGCMManager* manager =
        fake_cryptauth_gcm_manager_factory_->instance();
    ASSERT_TRUE(manager);
    EXPECT_TRUE(manager->IsListening());
    EXPECT_TRUE(manager->GetRegistrationId().empty());
    EXPECT_TRUE(manager->registration_in_progress());
    manager->CompleteRegistration(kTestCryptAuthGCMRegistrationId);
    EXPECT_FALSE(manager->registration_in_progress());
  }

  void SucceedClientAppMetadataFetch() {
    ASSERT_EQ(1u,
              fake_client_app_metadata_provider_->metadata_requests().size());
    EXPECT_EQ(kTestCryptAuthGCMRegistrationId,
              fake_client_app_metadata_provider_->metadata_requests()
                  .back()
                  .gcm_registration_id);
    std::move(
        fake_client_app_metadata_provider_->metadata_requests().back().callback)
        .Run(cryptauthv2::GetClientAppMetadataForTest());
  }

  void FinishInitialization() {
    // CryptAuth classes are expected to be created and initialized.
    EXPECT_TRUE(fake_cryptauth_enrollment_manager()->has_started());
    // If the device was already enrolled in CryptAuth, initialization should
    // now be complete; otherwise, enrollment needs to finish before
    // the flow has finished up.
    VerifyInitializationStatus(
        device_already_enrolled_in_cryptauth_ /* expected_to_be_initialized */);
    if (!device_already_enrolled_in_cryptauth_)
      return;
    // Now that the service is initialized, RemoteDeviceProvider is expected to
    // load all relevant RemoteDevice objects.
    fake_remote_device_provider_factory_->instance()
        ->NotifyObserversDeviceListChanged();
  }

  void VerifyInitializationStatus(bool expected_to_be_initialized) {
    // CryptAuthV2DeviceManager::Start() is called as the last step of the
    // initialization flow.
    if (features::ShouldUseV2DeviceSync()) {
      EXPECT_EQ(
          expected_to_be_initialized,
          fake_cryptauth_v2_device_manager_factory_->instance()->has_started());
    }
  }

  // Simulates an enrollment with success == |success|. If enrollment was not
  // yet in progress before this call, it is started before it is completed.
  void SimulateEnrollment(bool success) {
    FakeCryptAuthEnrollmentManager* enrollment_manager =
        fake_cryptauth_enrollment_manager();

    bool had_valid_enrollment_before_call =
        enrollment_manager->IsEnrollmentValid();

    if (!enrollment_manager->IsEnrollmentInProgress()) {
      enrollment_manager->ForceEnrollmentNow(
          cryptauth::InvocationReason::INVOCATION_REASON_MANUAL);
    }

    enrollment_manager->FinishActiveEnrollment(success);

    // If this was the first successful enrollment for this device,
    // RemoteDeviceProvider is expected to load all relevant RemoteDevice
    // objects.
    if (success && !had_valid_enrollment_before_call) {
      fake_remote_device_provider_factory_->instance()
          ->NotifyObserversDeviceListChanged();
    }
  }

  // Simulates a device sync with success == |success|. Optionally, if
  // |updated_devices| is provided, these devices will set on the
  // FakeRemoteDeviceProvider.
  void SimulateSync(bool success,
                    const multidevice::RemoteDeviceList& updated_devices =
                        multidevice::RemoteDeviceList()) {
    FakeCryptAuthV2DeviceManager* v2_device_manager =
        fake_cryptauth_v2_device_manager_factory_->instance();
    FakeRemoteDeviceProvider* remote_device_provider =
        fake_remote_device_provider_factory_->instance();

    if (features::ShouldUseV2DeviceSync()) {
      EXPECT_TRUE(v2_device_manager->IsDeviceSyncInProgress());
      v2_device_manager->FinishNextForcedDeviceSync(
          CryptAuthDeviceSyncResult(
              success ? CryptAuthDeviceSyncResult::ResultCode::kSuccess
                      : CryptAuthDeviceSyncResult::ResultCode::
                            kErrorSyncMetadataApiCallBadRequest,
              !updated_devices.empty(), std::nullopt /* client_directive */),
          base::Time::Now());
    }

    if (!updated_devices.empty()) {
      remote_device_provider->set_synced_remote_devices(updated_devices);
      remote_device_provider->NotifyObserversDeviceListChanged();
    }
  }

  void InitializeServiceSuccessfully() {
    // In most login scenarios the primary account is available immediately.
    MakePrimaryAccountAvailable();
    InitializeDeviceSync(true /* device_already_enrolled_in_cryptauth */);
    SucceedGcmRegistration();
    SucceedClientAppMetadataFetch();
    FinishInitialization();
    VerifyInitializationStatus(true /* expected_to_be_initialized */);

    base::RunLoop().RunUntilIdle();

    // Enrollment did not occur since the device was already in a valid state.
    EXPECT_EQ(0u, fake_device_sync_observer()->num_enrollment_events());

    // The initial set of synced devices was set.
    EXPECT_EQ(1u, fake_device_sync_observer()->num_sync_events());
  }

  const multidevice::RemoteDeviceList& test_devices() { return test_devices_; }

  const std::vector<cryptauth::ExternalDeviceInfo>& test_device_infos() {
    return test_device_infos_;
  }

  const std::vector<cryptauth::IneligibleDevice>& test_ineligible_devices() {
    return test_ineligible_devices_;
  }

  FakeDeviceSyncObserver* fake_device_sync_observer() {
    return fake_device_sync_observer_.get();
  }

  base::MockOneShotTimer* mock_timer() { return mock_timer_; }

  FakeClientAppMetadataProvider* fake_client_app_metadata_provider() {
    return fake_client_app_metadata_provider_.get();
  }

  FakeCryptAuthGCMManager* fake_cryptauth_gcm_manager() {
    return fake_cryptauth_gcm_manager_factory_->instance();
  }

  FakeCryptAuthEnrollmentManager* fake_cryptauth_enrollment_manager() {
    return fake_cryptauth_v2_enrollment_manager_factory_->instance();
  }

  FakeCryptAuthV2DeviceManager* fake_cryptauth_v2_device_manager() {
    return fake_cryptauth_v2_device_manager_factory_->instance();
  }

  FakeSoftwareFeatureManager* fake_software_feature_manager() {
    return fake_software_feature_manager_factory_->instance();
  }

  FakeCryptAuthFeatureStatusSetter* fake_feature_status_setter() {
    if (fake_feature_status_setter_factory_->instances().empty())
      return nullptr;

    EXPECT_EQ(1u, fake_feature_status_setter_factory_->instances().size());
    EXPECT_EQ(cryptauthv2::GetClientAppMetadataForTest().instance_id(),
              fake_device_notifier_factory_->last_instance_id());
    EXPECT_EQ(cryptauthv2::GetClientAppMetadataForTest().instance_id_token(),
              fake_device_notifier_factory_->last_instance_id_token());
    return fake_feature_status_setter_factory_->instances()[0];
  }

  const std::vector<mojom::NetworkRequestResult>& set_feature_status_results() {
    return set_feature_status_results_;
  }

  FakeCryptAuthDeviceNotifier* fake_device_notifier() {
    if (fake_device_notifier_factory_->instances().empty())
      return nullptr;

    EXPECT_EQ(1u, fake_device_notifier_factory_->instances().size());
    EXPECT_EQ(cryptauthv2::GetClientAppMetadataForTest().instance_id(),
              fake_device_notifier_factory_->last_instance_id());
    EXPECT_EQ(cryptauthv2::GetClientAppMetadataForTest().instance_id_token(),
              fake_device_notifier_factory_->last_instance_id_token());
    return fake_device_notifier_factory_->instances()[0];
  }

  const std::vector<mojom::NetworkRequestResult>& notify_devices_results() {
    return notify_devices_results_;
  }

  std::unique_ptr<mojom::NetworkRequestResult>
  GetLastSetSoftwareFeatureStateResponseAndReset() {
    return std::move(last_set_software_feature_state_response_);
  }

  std::unique_ptr<std::pair<mojom::NetworkRequestResult,
                            mojom::FindEligibleDevicesResponsePtr>>
  GetLastFindEligibleDevicesResponseAndReset() {
    return std::move(last_find_eligible_devices_response_);
  }

  // Verifies that API functions return error codes before initialization has
  // completed. This function should not be invoked after initialization.
  void VerifyApiFunctionsFailBeforeInitialization() {
    // Force*Now() functions return false when they are not handled.
    EXPECT_FALSE(CallForceEnrollmentNow());
    EXPECT_FALSE(CallForceSyncNow());

    // GetSyncedDevices() returns a null list before initialization.
    EXPECT_FALSE(CallGetSyncedDevices());

    // GetLocalDeviceMetadata() returns a null RemoteDevice before
    // initialization.
    EXPECT_FALSE(CallGetLocalDeviceMetadata());

    // GetGroupPrivateKeyStatus() and GetBetterTogetherMetadataStatus() both
    // should return kStatusUnavailableBecauseDeviceSyncIsNotInitialized.
    EXPECT_EQ(GroupPrivateKeyStatus::
                  kStatusUnavailableBecauseDeviceSyncIsNotInitialized,
              CallGetGroupPrivateKeyStatus());
    EXPECT_EQ(BetterTogetherMetadataStatus::
                  kStatusUnavailableBecauseDeviceSyncIsNotInitialized,
              CallGetBetterTogetherMetadataStatus());

    if (features::ShouldUseV1DeviceSync()) {
      // SetSoftwareFeatureState() should return a struct with the special
      // kErrorNotInitialized error code.
      CallSetSoftwareFeatureState(
          test_devices()[0].public_key,
          multidevice::SoftwareFeature::kBetterTogetherHost, true /* enabled */,
          true /* is_exclusive */);
      auto last_set_response = GetLastSetSoftwareFeatureStateResponseAndReset();
      EXPECT_EQ(mojom::NetworkRequestResult::kServiceNotYetInitialized,
                *last_set_response);
    }

    if (features::ShouldUseV2DeviceSync()) {
      // SetFeatureStatus() should return a struct with the special
      // kErrorNotInitialized error code.
      CallSetFeatureStatus(test_devices()[0].instance_id,
                           multidevice::SoftwareFeature::kBetterTogetherHost,
                           FeatureStatusChange::kEnableExclusively);
      EXPECT_EQ(1u, set_feature_status_results_.size());
      EXPECT_EQ(mojom::NetworkRequestResult::kServiceNotYetInitialized,
                set_feature_status_results_[0]);
    }

    if (features::ShouldUseV1DeviceSync()) {
      // FindEligibleDevices() should return a struct with the special
      // kErrorNotInitialized error code.
      CallFindEligibleDevices(
          multidevice::SoftwareFeature::kBetterTogetherHost);
      auto last_find_response = GetLastFindEligibleDevicesResponseAndReset();
      EXPECT_EQ(mojom::NetworkRequestResult::kServiceNotYetInitialized,
                last_find_response->first);
      EXPECT_FALSE(last_find_response->second /* response */);
    }

    if (features::ShouldUseV2DeviceSync()) {
      // NotifyDevices() should return a struct with the special
      // kErrorNotInitialized error code.
      CallNotifyDevices(
          {test_devices()[0].instance_id, test_devices()[1].instance_id},
          cryptauthv2::TargetService::DEVICE_SYNC,
          multidevice::SoftwareFeature::kBetterTogetherHost);
      EXPECT_EQ(1u, notify_devices_results_.size());
      EXPECT_EQ(mojom::NetworkRequestResult::kServiceNotYetInitialized,
                notify_devices_results_[0]);
    }

    // GetDebugInfo() returns a null DebugInfo before initialization.
    EXPECT_FALSE(CallGetDebugInfo());
  }

  void CallAddObserver() {
    base::RunLoop run_loop;
    device_sync_->AddObserver(
        fake_device_sync_observer_->GenerateRemote(),
        base::BindOnce(&DeviceSyncServiceTest::OnAddObserverCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  bool CallForceEnrollmentNow() {
    base::RunLoop run_loop;
    device_sync_->ForceEnrollmentNow(
        base::BindOnce(&DeviceSyncServiceTest::OnForceEnrollmentNowCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    if (fake_cryptauth_enrollment_manager()) {
      EXPECT_EQ(last_force_enrollment_now_result_,
                fake_cryptauth_enrollment_manager()->IsEnrollmentInProgress());
    }

    return last_force_enrollment_now_result_;
  }

  bool CallForceSyncNow() {
    FakeCryptAuthV2DeviceManager* v2_device_manager =
        fake_cryptauth_v2_device_manager_factory_->instance();

    size_t expected_num_v2_force_device_sync_now_calls = 0;
    if (v2_device_manager) {
      expected_num_v2_force_device_sync_now_calls =
          v2_device_manager->force_device_sync_now_requests().size();
    }

    base::RunLoop run_loop;
    device_sync_->ForceSyncNow(
        base::BindOnce(&DeviceSyncServiceTest::OnForceSyncNowCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    if (v2_device_manager) {
      if (last_force_sync_now_result_)
        ++expected_num_v2_force_device_sync_now_calls;

      EXPECT_EQ(expected_num_v2_force_device_sync_now_calls,
                v2_device_manager->force_device_sync_now_requests().size());
    }

    return last_force_sync_now_result_;
  }

  GroupPrivateKeyStatus CallGetGroupPrivateKeyStatus() {
    base::RunLoop run_loop;
    device_sync_->GetGroupPrivateKeyStatus(base::BindOnce(
        &DeviceSyncServiceTest::OnGetGroupPrivateKeyStatusCompleted,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return last_group_private_key_status_result_;
  }

  BetterTogetherMetadataStatus CallGetBetterTogetherMetadataStatus() {
    base::RunLoop run_loop;
    device_sync_->GetBetterTogetherMetadataStatus(base::BindOnce(
        &DeviceSyncServiceTest::OnGetBetterTogetherMetadataStatusCompleted,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return last_better_together_metadata_status_result_;
  }

  const std::optional<multidevice::RemoteDevice>& CallGetLocalDeviceMetadata() {
    base::RunLoop run_loop;
    device_sync_->GetLocalDeviceMetadata(base::BindOnce(
        &DeviceSyncServiceTest::OnGetLocalDeviceMetadataCompleted,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return last_local_device_metadata_result_;
  }

  const std::optional<multidevice::RemoteDeviceList>& CallGetSyncedDevices() {
    base::RunLoop run_loop;
    device_sync_->GetSyncedDevices(
        base::BindOnce(&DeviceSyncServiceTest::OnGetSyncedDevicesCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return last_synced_devices_result_;
  }

  void CallSetSoftwareFeatureState(
      const std::string& public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive) {
    base::RunLoop run_loop;
    FakeSoftwareFeatureManager* manager = fake_software_feature_manager();

    // If the manager has not yet been created, the service has not been
    // initialized. SetSoftwareFeatureState() is expected to respond
    // synchronously with an error.
    if (!manager) {
      device_sync_->SetSoftwareFeatureState(
          public_key, software_feature, enabled, is_exclusive,
          base::BindOnce(&DeviceSyncServiceTest::
                             OnSetSoftwareFeatureStateCompletedSynchronously,
                         base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();
      return;
    }

    // If the manager has been created, the service responds asynchronously.
    FakeSoftwareFeatureManagerDelegate delegate(run_loop.QuitClosure());
    fake_software_feature_manager_factory_->instance()->set_delegate(&delegate);

    device_sync_->SetSoftwareFeatureState(
        public_key, software_feature, enabled, is_exclusive,
        base::BindOnce(
            &DeviceSyncServiceTest::OnSetSoftwareFeatureStateCompleted,
            base::Unretained(this)));
    run_loop.Run();

    fake_software_feature_manager_factory_->instance()->set_delegate(nullptr);
  }

  void CallSetFeatureStatus(const std::string& device_instance_id,
                            multidevice::SoftwareFeature software_feature,
                            FeatureStatusChange status_change) {
    if (features::ShouldUseV1DeviceSync()) {
      CallSetFeatureStatusV1andV2DeviceSync(device_instance_id,
                                            software_feature, status_change);
      return;
    }

    CallSetFeatureStatusV2DeviceSyncOnly(device_instance_id, software_feature,
                                         status_change);
  }

  void CallFindEligibleDevices(multidevice::SoftwareFeature software_feature) {
    base::RunLoop run_loop;
    FakeSoftwareFeatureManager* manager = fake_software_feature_manager();

    // If the manager has not yet been created, the service has not been
    // initialized. FindEligibleDevices() is expected to respond synchronously
    // with an error.
    if (!manager) {
      device_sync_->FindEligibleDevices(
          software_feature,
          base::BindOnce(&DeviceSyncServiceTest::
                             OnFindEligibleDevicesCompletedSynchronously,
                         base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();
      return;
    }

    // If the manager has been created, the service responds asynchronously.
    FakeSoftwareFeatureManagerDelegate delegate(run_loop.QuitClosure());
    fake_software_feature_manager_factory_->instance()->set_delegate(&delegate);

    device_sync_->FindEligibleDevices(
        software_feature,
        base::BindOnce(&DeviceSyncServiceTest::OnFindEligibleDevicesCompleted,
                       base::Unretained(this)));
    run_loop.Run();

    fake_software_feature_manager_factory_->instance()->set_delegate(nullptr);
  }

  void CallNotifyDevices(const std::vector<std::string>& device_instance_ids,
                         cryptauthv2::TargetService target_service,
                         multidevice::SoftwareFeature feature) {
    base::RunLoop run_loop;

    // If the device notifier has not yet been created, the service has not been
    // initialized. NotifyDevices() is expected to respond synchronously with an
    // error.
    if (!fake_device_notifier()) {
      device_sync_->NotifyDevices(
          device_instance_ids, target_service, feature,
          base::BindOnce(
              &DeviceSyncServiceTest::OnNotifyDevicesCompletedSynchronously,
              base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();
      return;
    }

    FakeCryptAuthDeviceNotifierDelegate delegate(run_loop.QuitClosure());
    fake_device_notifier()->set_delegate(&delegate);
    device_sync_->NotifyDevices(
        device_instance_ids, target_service, feature,
        base::BindOnce(&DeviceSyncServiceTest::OnNotifyDevicesCompleted,
                       base::Unretained(this)));
    run_loop.Run();
    fake_device_notifier()->set_delegate(nullptr);
  }

  const std::optional<mojom::DebugInfo>& CallGetDebugInfo() {
    base::RunLoop run_loop;
    device_sync_->GetDebugInfo(
        base::BindOnce(&DeviceSyncServiceTest::OnGetDebugInfoCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return last_debug_info_result_;
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  void CallSetFeatureStatusV1andV2DeviceSync(
      const std::string& device_instance_id,
      multidevice::SoftwareFeature software_feature,
      FeatureStatusChange status_change) {
    base::RunLoop run_loop;
    FakeSoftwareFeatureManager* manager = fake_software_feature_manager();

    // If the manager has not yet been created, the service has not been
    // initialized. SetFeatureStatus() is expected to respond synchronously with
    // an error.
    if (!manager) {
      device_sync_->SetFeatureStatus(
          device_instance_id, software_feature, status_change,
          base::BindOnce(
              &DeviceSyncServiceTest::OnSetFeatureStatusCompletedSynchronously,
              base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();
      return;
    }

    // If the manager has been created, the service responds asynchronously.
    FakeSoftwareFeatureManagerDelegate delegate(run_loop.QuitClosure());
    fake_software_feature_manager_factory_->instance()->set_delegate(&delegate);

    device_sync_->SetFeatureStatus(
        device_instance_id, software_feature, status_change,
        base::BindOnce(&DeviceSyncServiceTest::OnSetFeatureStatusCompleted,
                       base::Unretained(this)));
    run_loop.Run();

    fake_software_feature_manager_factory_->instance()->set_delegate(nullptr);
  }

  void CallSetFeatureStatusV2DeviceSyncOnly(
      const std::string& device_instance_id,
      multidevice::SoftwareFeature software_feature,
      FeatureStatusChange status_change) {
    base::RunLoop run_loop;

    // If the feature setter has not yet been created, the service has not been
    // initialized. SetFeatureStatus() is expected to respond synchronously with
    // an error.
    if (!fake_feature_status_setter()) {
      device_sync_->SetFeatureStatus(
          device_instance_id, software_feature, status_change,
          base::BindOnce(
              &DeviceSyncServiceTest::OnSetFeatureStatusCompletedSynchronously,
              base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();
      return;
    }

    FakeCryptAuthFeatureStatusSetterDelegate delegate(run_loop.QuitClosure());
    fake_feature_status_setter()->set_delegate(&delegate);
    device_sync_->SetFeatureStatus(
        device_instance_id, software_feature, status_change,
        base::BindOnce(&DeviceSyncServiceTest::OnSetFeatureStatusCompleted,
                       base::Unretained(this)));
    run_loop.Run();

    fake_feature_status_setter()->set_delegate(nullptr);
  }

  void OnAddObserverCompleted(base::OnceClosure quit_closure) {
    std::move(quit_closure).Run();
  }

  void OnForceEnrollmentNowCompleted(base::OnceClosure quit_closure,
                                     bool success) {
    last_force_enrollment_now_result_ = success;
    std::move(quit_closure).Run();
  }

  void OnForceSyncNowCompleted(base::OnceClosure quit_closure, bool success) {
    last_force_sync_now_result_ = success;
    std::move(quit_closure).Run();
  }

  void OnGetGroupPrivateKeyStatusCompleted(base::OnceClosure quit_closure,
                                           GroupPrivateKeyStatus status) {
    last_group_private_key_status_result_ = status;
    std::move(quit_closure).Run();
  }

  void OnGetBetterTogetherMetadataStatusCompleted(
      base::OnceClosure quit_closure,
      BetterTogetherMetadataStatus status) {
    last_better_together_metadata_status_result_ = status;
    std::move(quit_closure).Run();
  }

  void OnGetLocalDeviceMetadataCompleted(
      base::OnceClosure quit_closure,
      const std::optional<multidevice::RemoteDevice>& local_device_metadata) {
    last_local_device_metadata_result_ = local_device_metadata;
    std::move(quit_closure).Run();
  }

  void OnGetSyncedDevicesCompleted(
      base::OnceClosure quit_closure,
      const std::optional<multidevice::RemoteDeviceList>& synced_devices) {
    last_synced_devices_result_ = synced_devices;
    std::move(quit_closure).Run();
  }

  void OnSetSoftwareFeatureStateCompleted(
      mojom::NetworkRequestResult result_code) {
    EXPECT_FALSE(last_set_software_feature_state_response_);
    last_set_software_feature_state_response_ =
        std::make_unique<mojom::NetworkRequestResult>(result_code);
  }

  void OnSetSoftwareFeatureStateCompletedSynchronously(
      base::OnceClosure quit_closure,
      mojom::NetworkRequestResult result_code) {
    OnSetSoftwareFeatureStateCompleted(result_code);
    std::move(quit_closure).Run();
  }

  void OnSetFeatureStatusCompleted(mojom::NetworkRequestResult result_code) {
    set_feature_status_results_.push_back(result_code);
  }

  void OnSetFeatureStatusCompletedSynchronously(
      base::OnceClosure quit_closure,
      mojom::NetworkRequestResult result_code) {
    OnSetFeatureStatusCompleted(result_code);
    std::move(quit_closure).Run();
  }

  void OnFindEligibleDevicesCompleted(
      mojom::NetworkRequestResult result_code,
      mojom::FindEligibleDevicesResponsePtr response) {
    EXPECT_FALSE(last_find_eligible_devices_response_);
    last_find_eligible_devices_response_ =
        std::make_unique<std::pair<mojom::NetworkRequestResult,
                                   mojom::FindEligibleDevicesResponsePtr>>(
            result_code, std::move(response));
  }

  void OnNotifyDevicesCompleted(mojom::NetworkRequestResult result_code) {
    notify_devices_results_.push_back(result_code);
  }

  void OnNotifyDevicesCompletedSynchronously(
      base::OnceClosure quit_closure,
      mojom::NetworkRequestResult result_code) {
    OnNotifyDevicesCompleted(result_code);
    std::move(quit_closure).Run();
  }

  void OnFindEligibleDevicesCompletedSynchronously(
      base::OnceClosure quit_closure,
      mojom::NetworkRequestResult result_code,
      mojom::FindEligibleDevicesResponsePtr response) {
    OnFindEligibleDevicesCompleted(result_code, std::move(response));
    std::move(quit_closure).Run();
  }

  void OnGetDebugInfoCompleted(base::OnceClosure quit_closure,
                               mojom::DebugInfoPtr debug_info) {
    EXPECT_FALSE(last_debug_info_result_);
    if (debug_info)
      last_debug_info_result_ = *debug_info;
    else
      last_debug_info_result_.reset();
    std::move(quit_closure).Run();
  }

  const base::test::TaskEnvironment task_environment_;
  const multidevice::RemoteDeviceList test_devices_;
  const std::vector<cryptauth::ExternalDeviceInfo> test_device_infos_;
  const std::vector<cryptauth::IneligibleDevice> test_ineligible_devices_;

  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_;
  std::unique_ptr<base::SimpleTestClock> simple_test_clock_;
  std::unique_ptr<FakeDeviceSyncImplFactory> fake_device_sync_impl_factory_;
  std::unique_ptr<FakeCryptAuthGCMManagerFactory>
      fake_cryptauth_gcm_manager_factory_;
  std::unique_ptr<FakeClientAppMetadataProvider>
      fake_client_app_metadata_provider_;
  std::unique_ptr<FakeCryptAuthKeyRegistryFactory>
      fake_cryptauth_key_registry_factory_;
  std::unique_ptr<FakeCryptAuthSchedulerFactory>
      fake_cryptauth_scheduler_factory_;
  std::unique_ptr<FakeCryptAuthV2EnrollmentManagerFactory>
      fake_cryptauth_v2_enrollment_manager_factory_;
  std::unique_ptr<FakeCryptAuthDeviceNotifierFactory>
      fake_device_notifier_factory_;
  std::unique_ptr<FakeCryptAuthFeatureStatusSetterFactory>
      fake_feature_status_setter_factory_;
  std::unique_ptr<FakeSoftwareFeatureManagerFactory>
      fake_software_feature_manager_factory_;
  std::unique_ptr<FakeCryptAuthDeviceRegistryFactory>
      fake_cryptauth_device_registry_factory_;
  std::unique_ptr<FakeCryptAuthV2DeviceManagerFactory>
      fake_cryptauth_v2_device_manager_factory_;
  std::unique_ptr<FakeRemoteDeviceProviderFactory>
      fake_remote_device_provider_factory_;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<gcm::FakeGCMDriver> fake_gcm_driver_;
  testing::NiceMock<MockInstanceIDDriver> fake_instance_id_driver_;
  std::unique_ptr<FakeGcmDeviceInfoProvider> fake_gcm_device_info_provider_;

  bool device_already_enrolled_in_cryptauth_;
  bool last_force_enrollment_now_result_;
  bool last_force_sync_now_result_;
  GroupPrivateKeyStatus last_group_private_key_status_result_;
  BetterTogetherMetadataStatus last_better_together_metadata_status_result_;
  std::optional<multidevice::RemoteDeviceList> last_synced_devices_result_;
  std::optional<multidevice::RemoteDevice> last_local_device_metadata_result_;
  std::unique_ptr<mojom::NetworkRequestResult>
      last_set_software_feature_state_response_;
  std::unique_ptr<std::pair<mojom::NetworkRequestResult,
                            mojom::FindEligibleDevicesResponsePtr>>
      last_find_eligible_devices_response_;
  std::optional<mojom::DebugInfo> last_debug_info_result_;
  std::vector<mojom::NetworkRequestResult> set_feature_status_results_;
  std::vector<mojom::NetworkRequestResult> notify_devices_results_;

  std::unique_ptr<FakeDeviceSyncObserver> fake_device_sync_observer_;
  std::unique_ptr<DeviceSyncBase> device_sync_;

  base::HistogramTester histogram_tester_;
};

TEST_F(DeviceSyncServiceTest, PrimaryAccountAvailableLater) {
  InitializeDeviceSync(true /* device_already_enrolled_in_cryptauth */);

  // API functions should fail if the primary account isn't ready yet.
  VerifyApiFunctionsFailBeforeInitialization();

  // No observer callbacks should have been invoked.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, fake_device_sync_observer()->num_enrollment_events());
  EXPECT_EQ(0u, fake_device_sync_observer()->num_sync_events());

  MakePrimaryAccountAvailable();
  SucceedGcmRegistration();
  SucceedClientAppMetadataFetch();
  FinishInitialization();
  VerifyInitializationStatus(true /* expected_to_be_initialized */);

  // API functions should be operational and synced devices should be available.
  EXPECT_TRUE(CallForceEnrollmentNow());
  EXPECT_TRUE(CallForceSyncNow());
  EXPECT_EQ(test_devices(), CallGetSyncedDevices());
}

TEST_F(DeviceSyncServiceTest, GcmRegistration) {
  MakePrimaryAccountAvailable();
  InitializeDeviceSync(true /* device_already_enrolled_in_cryptauth */);

  // Registers with GCM, failing twice then succeeding.
  VerifyApiFunctionsFailBeforeInitialization();
  ASSERT_TRUE(fake_cryptauth_gcm_manager());
  EXPECT_TRUE(fake_cryptauth_gcm_manager()->IsListening());
  for (size_t num_retries = 0; num_retries < 2; ++num_retries) {
    EXPECT_TRUE(fake_cryptauth_gcm_manager()->GetRegistrationId().empty());
    EXPECT_TRUE(fake_cryptauth_gcm_manager()->registration_in_progress());
    fake_cryptauth_gcm_manager()->CompleteRegistration(
        std::string() /* registration_id */);
    EXPECT_TRUE(mock_timer()->IsRunning());
    mock_timer()->Fire();
  }
  fake_cryptauth_gcm_manager()->CompleteRegistration(
      kTestCryptAuthGCMRegistrationId);
  EXPECT_FALSE(fake_cryptauth_gcm_manager()->registration_in_progress());

  SucceedClientAppMetadataFetch();
  FinishInitialization();
}

TEST_F(DeviceSyncServiceTest, GcmRegistration_SkipIfAlreadyRegistered) {
  // Assume GCM registration already happened. Then, no need to register again.
  SetInitialRegistrationId(kTestCryptAuthGCMRegistrationId);
  MakePrimaryAccountAvailable();
  InitializeDeviceSync(true /* device_already_enrolled_in_cryptauth */);
  SucceedClientAppMetadataFetch();
  FinishInitialization();
}

TEST_F(DeviceSyncServiceTest, GcmRegistration_RegisterAgainIfDeprecated) {
  // Assume GCM registration already happened, but with a deprecated
  // registration id. This time, we should fail.
  SetInitialRegistrationId(kTestDeprecatedCryptAuthGCMRegistrationId);
  MakePrimaryAccountAvailable();
  InitializeDeviceSync(true /* device_already_enrolled_in_cryptauth */);

  // Unlike in the previous case, GCM Registration should have been started
  // since our existing registration id is deprecated.
  ASSERT_TRUE(fake_cryptauth_gcm_manager()->registration_in_progress());
  fake_cryptauth_gcm_manager()->CompleteRegistration(
      kTestCryptAuthGCMRegistrationId);
  EXPECT_FALSE(fake_cryptauth_gcm_manager()->registration_in_progress());

  SucceedClientAppMetadataFetch();
  FinishInitialization();
}

TEST_F(DeviceSyncServiceTest, ClientAppMetadataFetch) {
  MakePrimaryAccountAvailable();
  InitializeDeviceSync(true /* device_already_enrolled_in_cryptauth */);
  SucceedGcmRegistration();

  // Fetch ClientAppMetadata by failing twice, timing out once, then succeeding.
  VerifyApiFunctionsFailBeforeInitialization();
  EXPECT_TRUE(mock_timer()->IsRunning());  // Timeout timer is running.
  for (size_t attempt = 1; attempt <= 4; ++attempt) {
    ASSERT_EQ(attempt,
              fake_client_app_metadata_provider()->metadata_requests().size());
    EXPECT_EQ(kTestCryptAuthGCMRegistrationId,
              fake_client_app_metadata_provider()
                  ->metadata_requests()[attempt - 1]
                  .gcm_registration_id);
    if (attempt <= 2) {
      // Fail and retry.
      std::move(fake_client_app_metadata_provider()
                    ->metadata_requests()[attempt - 1]
                    .callback)
          .Run(std::nullopt /* client_app_metadata */);
      EXPECT_TRUE(mock_timer()->IsRunning());  // Retry timer is running.
      mock_timer()->Fire();
    } else if (attempt == 3) {
      // Time out and retry.
      EXPECT_TRUE(mock_timer()->IsRunning());  // Timeout timer is running.
      mock_timer()->Fire();
      EXPECT_TRUE(mock_timer()->IsRunning());  // Retry timer is running.
      mock_timer()->Fire();
    } else {
      // Succeed.
      std::move(fake_client_app_metadata_provider()
                    ->metadata_requests()[attempt - 1]
                    .callback)
          .Run(cryptauthv2::GetClientAppMetadataForTest());
    }
  }

  FinishInitialization();
}

TEST_F(DeviceSyncServiceTest,
       DeviceNotAlreadyEnrolledInCryptAuth_FailsEnrollment) {
  MakePrimaryAccountAvailable();
  InitializeDeviceSync(false /* device_already_enrolled_in_cryptauth */);
  SucceedGcmRegistration();
  SucceedClientAppMetadataFetch();
  FinishInitialization();

  // Simulate enrollment failing.
  SimulateEnrollment(false /* success */);
  VerifyInitializationStatus(false /* success */);

  // Fail again; initialization still should not complete.
  SimulateEnrollment(false /* success */);
  VerifyInitializationStatus(false /* expected_to_be_initialized */);

  // Other API functions should still fail since initialization never completed.
  VerifyApiFunctionsFailBeforeInitialization();

  // No observer callbacks should have been invoked.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, fake_device_sync_observer()->num_enrollment_events());
  EXPECT_EQ(0u, fake_device_sync_observer()->num_sync_events());
}

TEST_F(DeviceSyncServiceTest,
       DeviceNotAlreadyEnrolledInCryptAuth_FailsEnrollment_ThenSucceeds) {
  MakePrimaryAccountAvailable();
  InitializeDeviceSync(false /* device_already_enrolled_in_cryptauth */);
  SucceedGcmRegistration();
  SucceedClientAppMetadataFetch();
  FinishInitialization();

  // Initialization has not yet completed, so no devices should be available.
  EXPECT_FALSE(CallGetSyncedDevices());

  // Simulate enrollment failing.
  SimulateEnrollment(false /* success */);
  VerifyInitializationStatus(false /* success */);

  // Simulate enrollment succeeding; this should result in a fully-initialized
  // service.
  SimulateEnrollment(true /* success */);
  VerifyInitializationStatus(true /* expected_to_be_initialized */);

  // Enrollment occurred successfully, and the initial set of synced devices was
  // set.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, fake_device_sync_observer()->num_enrollment_events());
  EXPECT_EQ(1u, fake_device_sync_observer()->num_sync_events());

  // Now that the service is initialized, API functions should be operation and
  // synced devices should be available.
  EXPECT_TRUE(CallForceEnrollmentNow());
  EXPECT_TRUE(CallForceSyncNow());
  EXPECT_EQ(test_devices(), CallGetSyncedDevices());
}

TEST_F(DeviceSyncServiceTest,
       DeviceAlreadyEnrolledInCryptAuth_InitializationFlow) {
  InitializeServiceSuccessfully();

  // Now that the service is initialized, API functions should be operation and
  // synced devices should be available.
  EXPECT_TRUE(CallForceEnrollmentNow());
  EXPECT_TRUE(CallForceSyncNow());
  EXPECT_EQ(test_devices(), CallGetSyncedDevices());
}

TEST_F(DeviceSyncServiceTest, EnrollAgainAfterInitialization) {
  InitializeServiceSuccessfully();

  // Force an enrollment.
  EXPECT_TRUE(CallForceEnrollmentNow());

  // Simulate that enrollment failing.
  SimulateEnrollment(false /* success */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, fake_device_sync_observer()->num_enrollment_events());

  // Force an enrollment again.
  EXPECT_TRUE(CallForceEnrollmentNow());

  // This time, simulate the enrollment succeeding.
  SimulateEnrollment(true /* success */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, fake_device_sync_observer()->num_enrollment_events());
}

TEST_F(DeviceSyncServiceTest, GetGroupPrivateKeyStatus) {
  if (!features::ShouldUseV2DeviceSync())
    return;

  InitializeServiceSuccessfully();
  EXPECT_EQ(GroupPrivateKeyStatus::kWaitingForGroupPrivateKey,
            CallGetGroupPrivateKeyStatus());
}

TEST_F(DeviceSyncServiceTest, GetBetterTogetherMetadataStatus) {
  if (!features::ShouldUseV2DeviceSync())
    return;

  InitializeServiceSuccessfully();
  EXPECT_EQ(BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata,
            CallGetBetterTogetherMetadataStatus());
}

TEST_F(DeviceSyncServiceTest, GetLocalDeviceMetadata) {
  InitializeServiceSuccessfully();

  const auto& result = CallGetLocalDeviceMetadata();
  EXPECT_TRUE(result);
  EXPECT_EQ(kLocalDevicePublicKey, result->public_key);
  // Note: In GenerateTestRemoteDevices(), the 0th test device is arbitrarily
  // chosen as the local device.
  EXPECT_EQ(test_devices()[0], *result);
}

TEST_F(DeviceSyncServiceTest, SyncedDeviceUpdates) {
  InitializeServiceSuccessfully();
  EXPECT_EQ(1u, fake_device_sync_observer()->num_sync_events());

  // Force a device sync.
  EXPECT_TRUE(CallForceSyncNow());

  // Simulate failed sync.
  SimulateSync(false /* success */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, fake_device_sync_observer()->num_sync_events());

  // Force a sync again.
  EXPECT_TRUE(CallForceSyncNow());

  // Simulate successful sync which does not change the synced device list.
  SimulateSync(true /* success */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, fake_device_sync_observer()->num_sync_events());

  // Force a sync again.
  EXPECT_TRUE(CallForceSyncNow());

  // Create a new list which is the same as the initial test devices except that
  // the first device is removed.
  multidevice::RemoteDeviceList updated_device_list(test_devices().begin() + 1,
                                                    test_devices().end());
  EXPECT_EQ(kNumTestDevices - 1, updated_device_list.size());

  // Simulate successful sync which does change the synced device list.
  SimulateSync(true /* success */, updated_device_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, fake_device_sync_observer()->num_sync_events());

  // The updated list should be available via GetSyncedDevices().
  EXPECT_EQ(updated_device_list, CallGetSyncedDevices());
}

TEST_F(DeviceSyncServiceTest, SetSoftwareFeatureState_Success) {
  if (!features::ShouldUseV1DeviceSync())
    return;

  InitializeServiceSuccessfully();

  const auto& set_software_calls =
      fake_software_feature_manager()->set_software_feature_state_calls();
  EXPECT_EQ(0u, set_software_calls.size());

  // Set the kBetterTogetherHost field to "supported".
  multidevice::RemoteDevice device_for_test = test_devices()[0];
  device_for_test
      .software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kSupported;
  EXPECT_TRUE(CallForceSyncNow());
  SimulateSync(true /* success */, {device_for_test});

  // Enable kBetterTogetherHost for the device.
  CallSetSoftwareFeatureState(device_for_test.public_key,
                              multidevice::SoftwareFeature::kBetterTogetherHost,
                              true /* enabled */, true /* is_exclusive */);
  EXPECT_EQ(1u, set_software_calls.size());
  EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
            set_software_calls[0]->software_feature);
  EXPECT_TRUE(set_software_calls[0]->enabled);
  EXPECT_TRUE(set_software_calls[0]->is_exclusive);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastSetSoftwareFeatureStateResponseAndReset());

  // Now, invoke the success callback.
  std::move(set_software_calls[0]->success_callback).Run();

  // The callback still has not yet been invoked, since a device sync has not
  // confirmed the feature state change yet.
  EXPECT_FALSE(GetLastSetSoftwareFeatureStateResponseAndReset());

  // Simulate a sync which includes the device with the correct "enabled" state.
  device_for_test
      .software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  base::RunLoop().RunUntilIdle();
  SimulateSync(true /* success */, {device_for_test});
  base::RunLoop().RunUntilIdle();

  auto last_response = GetLastSetSoftwareFeatureStateResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(mojom::NetworkRequestResult::kSuccess, *last_response);

  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result", false, 0);
  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result", true, 1);
  histogram_tester().ExpectTotalCount(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result."
      "FailureReason",
      0);
}

TEST_F(DeviceSyncServiceTest,
       SetSoftwareFeatureState_RequestSucceedsButDoesNotTakeEffect) {
  if (!features::ShouldUseV1DeviceSync())
    return;

  InitializeServiceSuccessfully();

  const auto& set_software_calls =
      fake_software_feature_manager()->set_software_feature_state_calls();
  EXPECT_EQ(0u, set_software_calls.size());

  // Set the kBetterTogetherHost field to "supported".
  multidevice::RemoteDevice device_for_test = test_devices()[0];
  device_for_test
      .software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kSupported;
  EXPECT_TRUE(CallForceSyncNow());
  SimulateSync(true /* success */, {device_for_test});

  // Enable kBetterTogetherHost for the device.
  CallSetSoftwareFeatureState(device_for_test.public_key,
                              multidevice::SoftwareFeature::kBetterTogetherHost,
                              true /* enabled */, true /* is_exclusive */);
  EXPECT_EQ(1u, set_software_calls.size());
  EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
            set_software_calls[0]->software_feature);
  EXPECT_TRUE(set_software_calls[0]->enabled);
  EXPECT_TRUE(set_software_calls[0]->is_exclusive);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastSetSoftwareFeatureStateResponseAndReset());

  // Fire the timer, simulating that the updated device metadata did not arrive
  // in time.
  mock_timer()->Fire();
  base::RunLoop().RunUntilIdle();

  auto last_response = GetLastSetSoftwareFeatureStateResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(mojom::NetworkRequestResult::kRequestSucceededButUnexpectedResult,
            *last_response);

  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result", false, 1);
  histogram_tester().ExpectTotalCount(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result."
      "FailureReason",
      1);
  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result", true, 0);
}

TEST_F(DeviceSyncServiceTest, SetSoftwareFeatureState_Error) {
  if (!features::ShouldUseV1DeviceSync())
    return;

  InitializeServiceSuccessfully();

  const auto& set_software_calls =
      fake_software_feature_manager()->set_software_feature_state_calls();
  EXPECT_EQ(0u, set_software_calls.size());

  // Set the kBetterTogetherHost field to "supported".
  multidevice::RemoteDevice device_for_test = test_devices()[0];
  device_for_test
      .software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kSupported;
  EXPECT_TRUE(CallForceSyncNow());
  SimulateSync(true /* success */, {device_for_test});

  // Enable kBetterTogetherHost for the device.
  CallSetSoftwareFeatureState(device_for_test.public_key,
                              multidevice::SoftwareFeature::kBetterTogetherHost,
                              true /* enabled */, true /* is_exclusive */);
  ASSERT_EQ(1u, set_software_calls.size());
  EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
            set_software_calls[0]->software_feature);
  EXPECT_TRUE(set_software_calls[0]->enabled);
  EXPECT_TRUE(set_software_calls[0]->is_exclusive);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastSetSoftwareFeatureStateResponseAndReset());

  // Now, invoke the error callback.
  std::move(set_software_calls[0]->error_callback)
      .Run(NetworkRequestError::kOffline);
  base::RunLoop().RunUntilIdle();
  auto last_response = GetLastSetSoftwareFeatureStateResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(mojom::NetworkRequestResult::kOffline, *last_response);

  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result", false, 1);
  histogram_tester().ExpectTotalCount(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result."
      "FailureReason",
      1);
  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.SetSoftwareFeatureState.Result", true, 0);
}

TEST_F(DeviceSyncServiceTest, SetFeatureStatus_Success) {
  if (!features::ShouldUseV2DeviceSync())
    return;

  InitializeServiceSuccessfully();

  if (features::ShouldUseV1DeviceSync()) {
    EXPECT_EQ(
        0u, fake_software_feature_manager()->set_feature_status_calls().size());
  } else {
    EXPECT_EQ(0u, fake_feature_status_setter()->requests().size());
  }

  multidevice::RemoteDevice device_for_test = test_devices()[0];

  // Exclusively enable kBetterTogetherHost for the device.
  CallSetFeatureStatus(device_for_test.instance_id,
                       multidevice::SoftwareFeature::kBetterTogetherHost,
                       FeatureStatusChange::kEnableExclusively);

  if (features::ShouldUseV1DeviceSync()) {
    EXPECT_EQ(
        1u, fake_software_feature_manager()->set_feature_status_calls().size());
    EXPECT_EQ(device_for_test.instance_id, fake_software_feature_manager()
                                               ->set_feature_status_calls()[0]
                                               ->device_id);
    EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
              fake_software_feature_manager()
                  ->set_feature_status_calls()[0]
                  ->feature);
    EXPECT_EQ(FeatureStatusChange::kEnableExclusively,
              fake_software_feature_manager()
                  ->set_feature_status_calls()[0]
                  ->status_change);
    EXPECT_TRUE(fake_software_feature_manager()
                    ->set_feature_status_calls()[0]
                    ->success_callback);
    EXPECT_TRUE(fake_software_feature_manager()
                    ->set_feature_status_calls()[0]
                    ->error_callback);
  } else {
    EXPECT_EQ(1u, fake_feature_status_setter()->requests().size());
    EXPECT_EQ(device_for_test.instance_id,
              fake_feature_status_setter()->requests()[0].device_id);
    EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
              fake_feature_status_setter()->requests()[0].feature);
    EXPECT_EQ(FeatureStatusChange::kEnableExclusively,
              fake_feature_status_setter()->requests()[0].status_change);
    EXPECT_TRUE(fake_feature_status_setter()->requests()[0].success_callback);
    EXPECT_TRUE(fake_feature_status_setter()->requests()[0].error_callback);
  }

  // The DeviceSyncImpl::SetFeatureStatus() callback has not yet been invoked.
  EXPECT_TRUE(set_feature_status_results().empty());

  // Now, invoke the success callback.
  if (features::ShouldUseV1DeviceSync()) {
    std::move(fake_software_feature_manager()
                  ->set_feature_status_calls()[0]
                  ->success_callback)
        .Run();
  } else {
    std::move(fake_feature_status_setter()->requests()[0].success_callback)
        .Run();
  }

  // The DeviceSyncImpl::SetFeatureStatus() callback still has not yet been
  // invoked since a device sync has not confirmed the feature state change yet.
  EXPECT_TRUE(set_feature_status_results().empty());

  // Simulate a sync which includes the device with the correct "enabled" state.
  device_for_test
      .software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  SimulateSync(true /* success */, {device_for_test});
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, set_feature_status_results().size());
  EXPECT_EQ(mojom::NetworkRequestResult::kSuccess,
            set_feature_status_results()[0]);
}

TEST_F(DeviceSyncServiceTest,
       SetFeatureStatus_RequestSucceedsButDoesNotTakeEffect) {
  if (!features::ShouldUseV2DeviceSync())
    return;

  InitializeServiceSuccessfully();

  // Expected device feature states after SetFeatureStatus() calls:
  // * Device 0 has kSmartLockHost disabled.
  // * Device 1 has kSmartLockHost disabled.
  // * Device 2 has kBetterTogetherHost enabled exclusively.
  // * Device 3 has kInstantTetheringHost enabled.
  // * Device 4 has kMessagesForWebHost disabled.
  multidevice::RemoteDeviceList expected_remote_devices =
      multidevice::CreateRemoteDeviceListForTest(5u);
  expected_remote_devices[0]
      .software_features[multidevice::SoftwareFeature::kSmartLockHost] =
      multidevice::SoftwareFeatureState::kSupported;
  expected_remote_devices[1]
      .software_features[multidevice::SoftwareFeature::kSmartLockHost] =
      multidevice::SoftwareFeatureState::kSupported;
  expected_remote_devices[2]
      .software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  expected_remote_devices[3]
      .software_features[multidevice::SoftwareFeature::kInstantTetheringHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  expected_remote_devices[4]
      .software_features[multidevice::SoftwareFeature::kMessagesForWebHost] =
      multidevice::SoftwareFeatureState::kSupported;

  CallSetFeatureStatus(expected_remote_devices[0].instance_id,
                       multidevice::SoftwareFeature::kSmartLockHost,
                       FeatureStatusChange::kDisable);
  CallSetFeatureStatus(expected_remote_devices[1].instance_id,
                       multidevice::SoftwareFeature::kSmartLockHost,
                       FeatureStatusChange::kDisable);
  CallSetFeatureStatus(expected_remote_devices[2].instance_id,
                       multidevice::SoftwareFeature::kBetterTogetherHost,
                       FeatureStatusChange::kEnableExclusively);
  CallSetFeatureStatus(expected_remote_devices[3].instance_id,
                       multidevice::SoftwareFeature::kInstantTetheringHost,
                       FeatureStatusChange::kEnableNonExclusively);
  CallSetFeatureStatus(expected_remote_devices[4].instance_id,
                       multidevice::SoftwareFeature::kMessagesForWebHost,
                       FeatureStatusChange::kDisable);

  if (features::ShouldUseV1DeviceSync()) {
    EXPECT_EQ(
        5u, fake_software_feature_manager()->set_feature_status_calls().size());
  } else {
    EXPECT_EQ(5u, fake_feature_status_setter()->requests().size());
  }

  // The DeviceSyncImpl::SetFeatureStatus() callbacks have not yet been invoked.
  EXPECT_TRUE(set_feature_status_results().empty());

  // Now, invoke the success callbacks.
  for (size_t i = 0; i < 5u; ++i) {
    if (features::ShouldUseV1DeviceSync()) {
      std::move(fake_software_feature_manager()
                    ->set_feature_status_calls()[i]
                    ->success_callback)
          .Run();
    } else {
      std::move(fake_feature_status_setter()->requests()[i].success_callback)
          .Run();
    }
  }

  // The DeviceSyncImpl::SetFeatureStatus() callbacks still have not been
  // invoked since a DeviceSync has not confirmed any of the requested feature
  // state changes yet.
  EXPECT_TRUE(set_feature_status_results().empty());

  // Simulate a DeviceSync which returns unexpected device feature states:
  // * Device 0 not in list of devices.
  // * Device 1 missing kSmartLockHost entry in the feature list.
  // * Device 2 has kBetterTogetherHost enabled but not exclusively since device
  //   1 also has it enabled.
  // * Device 3 does not have kInstantTetheringHost enabled.
  // * Device 4 does not have kMessagesForWebHost disabled.
  multidevice::RemoteDeviceList remote_devices_from_first_sync =
      multidevice::CreateRemoteDeviceListForTest(5u);
  remote_devices_from_first_sync[1]
      .software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  remote_devices_from_first_sync[2]
      .software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  remote_devices_from_first_sync[3]
      .software_features[multidevice::SoftwareFeature::kInstantTetheringHost] =
      multidevice::SoftwareFeatureState::kSupported;
  remote_devices_from_first_sync[4]
      .software_features[multidevice::SoftwareFeature::kMessagesForWebHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  remote_devices_from_first_sync.erase(remote_devices_from_first_sync.begin());

  SimulateSync(true /* success */, remote_devices_from_first_sync);
  base::RunLoop().RunUntilIdle();

  // The DeviceSyncImpl::SetFeatureStatus() callbacks still have not yet been
  // invoked since the latest DeviceSync did not reflect the requested feature
  // state changes.
  EXPECT_EQ(0u, set_feature_status_results().size());

  // Simulate a DeviceSync which returns the expected device feature states:
  EXPECT_TRUE(CallForceSyncNow());
  SimulateSync(true /* success */, expected_remote_devices);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(5u, set_feature_status_results().size());
  for (mojom::NetworkRequestResult result : set_feature_status_results())
    EXPECT_EQ(mojom::NetworkRequestResult::kSuccess, result);
}

TEST_F(DeviceSyncServiceTest, SetFeatureStatus_Error) {
  if (!features::ShouldUseV2DeviceSync())
    return;

  InitializeServiceSuccessfully();

  multidevice::RemoteDevice device_for_test = test_devices()[0];

  // Attempt to exclusively enable kBetterTogetherHost for the device.
  CallSetFeatureStatus(device_for_test.instance_id,
                       multidevice::SoftwareFeature::kBetterTogetherHost,
                       FeatureStatusChange::kEnableExclusively);

  // The DeviceSyncImpl::SetFeatureStatus() callback has not yet been invoked.
  EXPECT_TRUE(set_feature_status_results().empty());

  // Now, invoke the error callback.
  if (features::ShouldUseV1DeviceSync()) {
    std::move(fake_software_feature_manager()
                  ->set_feature_status_calls()[0]
                  ->error_callback)
        .Run(NetworkRequestError::kBadRequest);
  } else {
    std::move(fake_feature_status_setter()->requests()[0].error_callback)
        .Run(NetworkRequestError::kBadRequest);
  }

  // The DeviceSyncImpl::SetFeatureStatus() callback is invoked with the same
  // error code.
  ASSERT_EQ(1u, set_feature_status_results().size());
  EXPECT_EQ(mojom::NetworkRequestResult::kBadRequest,
            set_feature_status_results()[0]);
}

TEST_F(DeviceSyncServiceTest, FindEligibleDevices) {
  if (!features::ShouldUseV1DeviceSync())
    return;

  InitializeServiceSuccessfully();

  const auto& find_eligible_calls =
      fake_software_feature_manager()->find_eligible_multidevice_host_calls();
  EXPECT_EQ(0u, find_eligible_calls.size());

  // Find devices which are kBetterTogetherHost.
  CallFindEligibleDevices(multidevice::SoftwareFeature::kBetterTogetherHost);
  EXPECT_EQ(1u, find_eligible_calls.size());
  EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
            find_eligible_calls[0]->software_feature);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastFindEligibleDevicesResponseAndReset());

  // Now, invoke the success callback, simultating that device 0 is eligible and
  // devices 1-4 are not.
  std::move(find_eligible_calls[0]->success_callback)
      .Run(std::vector<cryptauth::ExternalDeviceInfo>(
               test_device_infos().begin(), test_device_infos().begin()),
           std::vector<cryptauth::IneligibleDevice>(
               test_ineligible_devices().begin() + 1,
               test_ineligible_devices().end()));
  base::RunLoop().RunUntilIdle();
  auto last_response = GetLastFindEligibleDevicesResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(mojom::NetworkRequestResult::kSuccess, last_response->first);
  EXPECT_EQ(last_response->second->eligible_devices,
            multidevice::RemoteDeviceList(test_devices().begin(),
                                          test_devices().begin()));
  EXPECT_EQ(last_response->second->ineligible_devices,
            multidevice::RemoteDeviceList(test_devices().begin() + 1,
                                          test_devices().end()));

  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.FindEligibleDevices.Result", false, 0);
  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.FindEligibleDevices.Result", true, 1);

  // Find devices which are BETTER_TOGETHER_HOSTs again.
  CallFindEligibleDevices(multidevice::SoftwareFeature::kBetterTogetherHost);
  EXPECT_EQ(2u, find_eligible_calls.size());
  EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
            find_eligible_calls[1]->software_feature);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastFindEligibleDevicesResponseAndReset());

  // Now, invoke the error callback.
  std::move(find_eligible_calls[1]->error_callback)
      .Run(NetworkRequestError::kOffline);
  base::RunLoop().RunUntilIdle();
  last_response = GetLastFindEligibleDevicesResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(mojom::NetworkRequestResult::kOffline, last_response->first);
  EXPECT_FALSE(last_response->second /* response */);

  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.FindEligibleDevices.Result", false, 1);
  histogram_tester().ExpectTotalCount(
      "MultiDevice.DeviceSyncService.FindEligibleDevices.Result."
      "FailureReason",
      1);
  histogram_tester().ExpectBucketCount<bool>(
      "MultiDevice.DeviceSyncService.FindEligibleDevices.Result", true, 1);
}

TEST_F(DeviceSyncServiceTest, NotifyDevices_Success) {
  if (!features::ShouldUseV2DeviceSync())
    return;

  InitializeServiceSuccessfully();

  EXPECT_EQ(0u, fake_device_notifier()->requests().size());

  std::vector<std::string> device_instance_ids = {
      test_devices()[0].instance_id, test_devices()[1].instance_id};

  CallNotifyDevices(device_instance_ids,
                    cryptauthv2::TargetService::DEVICE_SYNC,
                    multidevice::SoftwareFeature::kBetterTogetherHost);
  EXPECT_EQ(1u, fake_device_notifier()->requests().size());
  EXPECT_EQ(device_instance_ids,
            fake_device_notifier()->requests()[0].device_ids);
  EXPECT_EQ(cryptauthv2::TargetService::DEVICE_SYNC,
            fake_device_notifier()->requests()[0].target_service);
  EXPECT_EQ(CryptAuthFeatureType::kBetterTogetherHostEnabled,
            fake_device_notifier()->requests()[0].feature_type);
  EXPECT_TRUE(fake_device_notifier()->requests()[0].success_callback);
  EXPECT_TRUE(fake_device_notifier()->requests()[0].error_callback);

  // The DeviceSyncImpl::NotifyDevices() callback has not yet been invoked.
  EXPECT_TRUE(notify_devices_results().empty());

  // Now, invoke the CryptAuthDeviceNotifier success callback.
  std::move(fake_device_notifier()->requests()[0].success_callback).Run();

  // The DeviceSyncImpl::NotifyDevices() callback should have been invoked.
  ASSERT_EQ(1u, notify_devices_results().size());
  EXPECT_EQ(mojom::NetworkRequestResult::kSuccess, notify_devices_results()[0]);
}

TEST_F(DeviceSyncServiceTest, NotifyDevices_Error) {
  if (!features::ShouldUseV2DeviceSync())
    return;

  InitializeServiceSuccessfully();

  CallNotifyDevices(
      {test_devices()[0].instance_id, test_devices()[1].instance_id},
      cryptauthv2::TargetService::DEVICE_SYNC,
      multidevice::SoftwareFeature::kBetterTogetherHost);

  // The DeviceSyncImpl::NotifyDevices() callback has not yet been invoked.
  EXPECT_TRUE(notify_devices_results().empty());

  // Now, invoke the CryptAuthDeviceNotifier error callback.
  std::move(fake_device_notifier()->requests()[0].error_callback)
      .Run(NetworkRequestError::kBadRequest);

  // The DeviceSyncImpl::NotifyDevices() callback is invoked with the same
  // error code.
  ASSERT_EQ(1u, notify_devices_results().size());
  EXPECT_EQ(mojom::NetworkRequestResult::kBadRequest,
            notify_devices_results()[0]);
}

TEST_F(DeviceSyncServiceTest, GetDebugInfo) {
  static const base::TimeDelta kTimeBetweenEpochAndLastEnrollment =
      base::Days(365 * 50);  // 50 years
  static const base::TimeDelta kTimeUntilNextEnrollment = base::Days(10);

  static const base::TimeDelta kTimeBetweenEpochAndLastSync =
      base::Days(366 * 50);  // 50 years and 1 day
  static const base::TimeDelta kTimeUntilNextSync = base::Days(11);

  InitializeServiceSuccessfully();

  fake_cryptauth_enrollment_manager()->set_last_enrollment_time(
      base::Time::FromDeltaSinceWindowsEpoch(
          kTimeBetweenEpochAndLastEnrollment));
  fake_cryptauth_enrollment_manager()->set_time_to_next_attempt(
      kTimeUntilNextEnrollment);
  fake_cryptauth_enrollment_manager()->set_is_recovering_from_failure(false);
  fake_cryptauth_enrollment_manager()->set_is_enrollment_in_progress(true);

  if (features::ShouldUseV2DeviceSync()) {
    fake_cryptauth_v2_device_manager()->ForceDeviceSyncNow(
        cryptauthv2::ClientMetadata::MANUAL /* invocation_reason */,
        std::nullopt /* session_id */);
    fake_cryptauth_v2_device_manager()->FinishNextForcedDeviceSync(
        CryptAuthDeviceSyncResult(
            CryptAuthDeviceSyncResult::ResultCode::kSuccess,
            true /* did_device_registry_change */,
            std::nullopt /* client_directive */),
        base::Time::FromDeltaSinceWindowsEpoch(
            kTimeBetweenEpochAndLastSync) /* device_sync_finish_time */);
    fake_cryptauth_v2_device_manager()->set_time_to_next_attempt(
        kTimeUntilNextSync);
  }

  const auto& result = CallGetDebugInfo();
  EXPECT_TRUE(result);
  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                kTimeBetweenEpochAndLastEnrollment),
            result->last_enrollment_time);
  EXPECT_EQ(base::TimeDelta(kTimeUntilNextEnrollment),
            result->time_to_next_enrollment_attempt);
  EXPECT_FALSE(result->is_recovering_from_enrollment_failure);
  EXPECT_TRUE(result->is_enrollment_in_progress);
  EXPECT_EQ(
      base::Time::FromDeltaSinceWindowsEpoch(kTimeBetweenEpochAndLastSync),
      result->last_sync_time);
  EXPECT_EQ(base::TimeDelta(kTimeUntilNextSync),
            result->time_to_next_sync_attempt);
  EXPECT_FALSE(result->is_recovering_from_sync_failure);
  EXPECT_FALSE(result->is_sync_in_progress);
}

}  // namespace device_sync

}  // namespace ash
