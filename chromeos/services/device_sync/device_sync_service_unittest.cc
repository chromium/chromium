// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/services/device_sync/device_sync_impl.h"
#include "chromeos/services/device_sync/device_sync_service.h"
#include "chromeos/services/device_sync/fake_device_sync_observer.h"
#include "chromeos/services/device_sync/public/mojom/constants.mojom.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/cryptauth/cryptauth_device_manager_impl.h"
#include "components/cryptauth/cryptauth_enrollment_manager_impl.h"
#include "components/cryptauth/cryptauth_gcm_manager_impl.h"
#include "components/cryptauth/fake_cryptauth_device_manager.h"
#include "components/cryptauth/fake_cryptauth_enrollment_manager.h"
#include "components/cryptauth/fake_cryptauth_gcm_manager.h"
#include "components/cryptauth/fake_gcm_device_info_provider.h"
#include "components/cryptauth/fake_remote_device_provider.h"
#include "components/cryptauth/fake_software_feature_manager.h"
#include "components/cryptauth/remote_device_provider_impl.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/cryptauth/software_feature_manager_impl.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kTestEmail[] = "example@gmail.com";
const char kTestGcmDeviceInfoLongDeviceId[] = "longDeviceId";
const char kTestCryptAuthGCMRegistrationId[] = "cryptAuthRegistrationId";
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

cryptauth::RemoteDeviceList GenerateTestRemoteDevices() {
  cryptauth::RemoteDeviceList devices =
      cryptauth::CreateRemoteDeviceListForTest(kNumTestDevices);

  // One of the synced devices refers to the current (i.e., local) device.
  // Arbitrarily choose the 0th device as the local one and set its public key
  // accordingly.
  devices[0].public_key = kLocalDevicePublicKey;

  return devices;
}

std::vector<cryptauth::ExternalDeviceInfo> GenerateTestExternalDeviceInfos(
    const cryptauth::RemoteDeviceList& remote_devices) {
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
    : public cryptauth::FakeSoftwareFeatureManager::Delegate {
 public:
  explicit FakeSoftwareFeatureManagerDelegate(
      base::Closure on_delegate_call_closure)
      : on_delegate_call_closure_(on_delegate_call_closure) {}

  ~FakeSoftwareFeatureManagerDelegate() override = default;

  // cryptauth::FakeSoftwareFeatureManager::Delegate:
  void OnSetSoftwareFeatureStateCalled() override {
    on_delegate_call_closure_.Run();
  }

  void OnFindEligibleDevicesCalled() override {
    on_delegate_call_closure_.Run();
  }

 private:
  base::Closure on_delegate_call_closure_;
};

class FakeCryptAuthGCMManagerFactory
    : public cryptauth::CryptAuthGCMManagerImpl::Factory {
 public:
  FakeCryptAuthGCMManagerFactory(gcm::FakeGCMDriver* fake_gcm_driver,
                                 TestingPrefServiceSimple* test_pref_service)
      : fake_gcm_driver_(fake_gcm_driver),
        test_pref_service_(test_pref_service) {}

  ~FakeCryptAuthGCMManagerFactory() override = default;

  cryptauth::FakeCryptAuthGCMManager* instance() { return instance_; }

  // cryptauth::CryptAuthGCMManagerImpl::Factory:
  std::unique_ptr<cryptauth::CryptAuthGCMManager> BuildInstance(
      gcm::GCMDriver* gcm_driver,
      PrefService* pref_service) override {
    EXPECT_EQ(fake_gcm_driver_, gcm_driver);
    EXPECT_EQ(test_pref_service_, pref_service);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<cryptauth::FakeCryptAuthGCMManager>(
        kTestCryptAuthGCMRegistrationId);
    instance_ = instance.get();

    return std::move(instance);
  }

 private:
  gcm::FakeGCMDriver* fake_gcm_driver_;
  TestingPrefServiceSimple* test_pref_service_;

  cryptauth::FakeCryptAuthGCMManager* instance_ = nullptr;
};

class FakeCryptAuthDeviceManagerFactory
    : public cryptauth::CryptAuthDeviceManagerImpl::Factory {
 public:
  FakeCryptAuthDeviceManagerFactory(
      base::SimpleTestClock* simple_test_clock,
      FakeCryptAuthGCMManagerFactory* fake_cryptauth_gcm_manager_factory,
      TestingPrefServiceSimple* test_pref_service)
      : simple_test_clock_(simple_test_clock),
        fake_cryptauth_gcm_manager_factory_(fake_cryptauth_gcm_manager_factory),
        test_pref_service_(test_pref_service) {}

  ~FakeCryptAuthDeviceManagerFactory() override = default;

  cryptauth::FakeCryptAuthDeviceManager* instance() { return instance_; }

  // cryptauth::CryptAuthDeviceManagerImpl::Factory:
  std::unique_ptr<cryptauth::CryptAuthDeviceManager> BuildInstance(
      base::Clock* clock,
      cryptauth::CryptAuthClientFactory* client_factory,
      cryptauth::CryptAuthGCMManager* gcm_manager,
      PrefService* pref_service) override {
    EXPECT_EQ(simple_test_clock_, clock);
    EXPECT_EQ(fake_cryptauth_gcm_manager_factory_->instance(), gcm_manager);
    EXPECT_EQ(test_pref_service_, pref_service);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<cryptauth::FakeCryptAuthDeviceManager>();
    instance_ = instance.get();

    return std::move(instance);
  }

 private:
  base::SimpleTestClock* simple_test_clock_;
  FakeCryptAuthGCMManagerFactory* fake_cryptauth_gcm_manager_factory_;
  TestingPrefServiceSimple* test_pref_service_;

  cryptauth::FakeCryptAuthDeviceManager* instance_ = nullptr;
};

class FakeCryptAuthEnrollmentManagerFactory
    : public cryptauth::CryptAuthEnrollmentManagerImpl::Factory {
 public:
  FakeCryptAuthEnrollmentManagerFactory(
      base::SimpleTestClock* simple_test_clock,
      FakeCryptAuthGCMManagerFactory* fake_cryptauth_gcm_manager_factory,
      TestingPrefServiceSimple* test_pref_service)
      : simple_test_clock_(simple_test_clock),
        fake_cryptauth_gcm_manager_factory_(fake_cryptauth_gcm_manager_factory),
        test_pref_service_(test_pref_service) {}

  ~FakeCryptAuthEnrollmentManagerFactory() override = default;

  void set_device_already_enrolled_in_cryptauth(
      bool device_already_enrolled_in_cryptauth) {
    device_already_enrolled_in_cryptauth_ =
        device_already_enrolled_in_cryptauth;
  }

  cryptauth::FakeCryptAuthEnrollmentManager* instance() { return instance_; }

  // cryptauth::CryptAuthEnrollmentManagerImpl::Factory:
  std::unique_ptr<cryptauth::CryptAuthEnrollmentManager> BuildInstance(
      base::Clock* clock,
      std::unique_ptr<cryptauth::CryptAuthEnrollerFactory> enroller_factory,
      std::unique_ptr<cryptauth::SecureMessageDelegate> secure_message_delegate,
      const cryptauth::GcmDeviceInfo& device_info,
      cryptauth::CryptAuthGCMManager* gcm_manager,
      PrefService* pref_service) override {
    EXPECT_EQ(simple_test_clock_, clock);
    EXPECT_EQ(kTestGcmDeviceInfoLongDeviceId, device_info.long_device_id());
    EXPECT_EQ(fake_cryptauth_gcm_manager_factory_->instance(), gcm_manager);
    EXPECT_EQ(test_pref_service_, pref_service);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance =
        std::make_unique<cryptauth::FakeCryptAuthEnrollmentManager>();
    instance->set_user_public_key(kLocalDevicePublicKey);
    instance->set_is_enrollment_valid(device_already_enrolled_in_cryptauth_);
    instance_ = instance.get();

    return std::move(instance);
  }

 private:
  base::SimpleTestClock* simple_test_clock_;
  FakeCryptAuthGCMManagerFactory* fake_cryptauth_gcm_manager_factory_;
  TestingPrefServiceSimple* test_pref_service_;

  bool device_already_enrolled_in_cryptauth_ = false;
  cryptauth::FakeCryptAuthEnrollmentManager* instance_ = nullptr;
};

class FakeRemoteDeviceProviderFactory
    : public cryptauth::RemoteDeviceProviderImpl::Factory {
 public:
  FakeRemoteDeviceProviderFactory(
      const cryptauth::RemoteDeviceList& initial_devices,
      identity::IdentityManager* identity_manager,
      FakeCryptAuthDeviceManagerFactory* fake_cryptauth_device_manager_factory,
      FakeCryptAuthEnrollmentManagerFactory*
          fake_cryptauth_enrollment_manager_factory)
      : initial_devices_(initial_devices),
        identity_manager_(identity_manager),
        fake_cryptauth_device_manager_factory_(
            fake_cryptauth_device_manager_factory),
        fake_cryptauth_enrollment_manager_factory_(
            fake_cryptauth_enrollment_manager_factory) {}

  ~FakeRemoteDeviceProviderFactory() override = default;

  cryptauth::FakeRemoteDeviceProvider* instance() { return instance_; }

  // cryptauth::RemoteDeviceProviderImpl::Factory:
  std::unique_ptr<cryptauth::RemoteDeviceProvider> BuildInstance(
      cryptauth::CryptAuthDeviceManager* device_manager,
      const std::string& user_id,
      const std::string& user_private_key) override {
    EXPECT_EQ(fake_cryptauth_device_manager_factory_->instance(),
              device_manager);
    EXPECT_EQ(identity_manager_->GetPrimaryAccountId(), user_id);
    EXPECT_EQ(fake_cryptauth_enrollment_manager_factory_->instance()
                  ->GetUserPrivateKey(),
              user_private_key);

    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<cryptauth::FakeRemoteDeviceProvider>();
    instance->set_synced_remote_devices(initial_devices_);
    instance_ = instance.get();

    return std::move(instance);
  }

 private:
  const cryptauth::RemoteDeviceList& initial_devices_;

  identity::IdentityManager* identity_manager_;
  FakeCryptAuthDeviceManagerFactory* fake_cryptauth_device_manager_factory_;
  FakeCryptAuthEnrollmentManagerFactory*
      fake_cryptauth_enrollment_manager_factory_;

  cryptauth::FakeRemoteDeviceProvider* instance_ = nullptr;
};

class FakeSoftwareFeatureManagerFactory
    : public cryptauth::SoftwareFeatureManagerImpl::Factory {
 public:
  FakeSoftwareFeatureManagerFactory() = default;
  ~FakeSoftwareFeatureManagerFactory() override = default;

  cryptauth::FakeSoftwareFeatureManager* instance() { return instance_; }

  // cryptauth::SoftwareFeatureManagerImpl::Factory:
  std::unique_ptr<cryptauth::SoftwareFeatureManager> BuildInstance(
      cryptauth::CryptAuthClientFactory* cryptauth_client_factory) override {
    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<cryptauth::FakeSoftwareFeatureManager>();
    instance_ = instance.get();

    return std::move(instance);
  }

 private:
  cryptauth::FakeSoftwareFeatureManager* instance_ = nullptr;
};

}  // namespace

class DeviceSyncServiceTest : public testing::Test {
 public:
  class FakePrefConnectionDelegate
      : public DeviceSyncImpl::PrefConnectionDelegate {
   public:
    FakePrefConnectionDelegate(
        std::unique_ptr<TestingPrefServiceSimple> test_pref_service)
        : test_pref_service_(std::move(test_pref_service)),
          test_pref_registry_(
              base::WrapRefCounted(test_pref_service_->registry())) {}

    ~FakePrefConnectionDelegate() override = default;

    void InvokePendingCallback() {
      EXPECT_FALSE(pending_callback_.is_null());
      std::move(pending_callback_).Run(std::move(test_pref_service_));

      // Note: |pending_callback_| was passed from within the service, so it is
      // necessary to let the rest of the current RunLoop run to ensure that
      // the callback is executed before returning from this function.
      base::RunLoop().RunUntilIdle();
    }

    bool HasStartedPrefConnection() {
      return HasFinishedPrefConnection() || !pending_callback_.is_null();
    }

    bool HasFinishedPrefConnection() { return !test_pref_service_.get(); }

    // DeviceSyncImpl::PrefConnectionDelegate:
    scoped_refptr<PrefRegistrySimple> CreatePrefRegistry() override {
      return test_pref_registry_;
    }

    void ConnectToPrefService(service_manager::Connector* connector,
                              scoped_refptr<PrefRegistrySimple> pref_registry,
                              prefs::ConnectCallback callback) override {
      EXPECT_EQ(test_pref_service_->registry(), pref_registry.get());
      pending_callback_ = std::move(callback);
    }

   private:
    std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
    scoped_refptr<PrefRegistrySimple> test_pref_registry_;

    prefs::ConnectCallback pending_callback_;
  };

  class FakeDeviceSyncImplFactory : public DeviceSyncImpl::Factory {
   public:
    FakeDeviceSyncImplFactory(
        std::unique_ptr<FakePrefConnectionDelegate>
            fake_pref_connection_delegate,
        std::unique_ptr<base::MockOneShotTimer> mock_timer,
        base::SimpleTestClock* simple_test_clock)
        : fake_pref_connection_delegate_(
              std::move(fake_pref_connection_delegate)),
          mock_timer_(std::move(mock_timer)),
          simple_test_clock_(simple_test_clock) {}

    ~FakeDeviceSyncImplFactory() override = default;

    // DeviceSyncImpl::Factory:
    std::unique_ptr<DeviceSyncBase> BuildInstance(
        identity::IdentityManager* identity_manager,
        gcm::GCMDriver* gcm_driver,
        service_manager::Connector* connector,
        const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::unique_ptr<base::OneShotTimer> timer) override {
      return base::WrapUnique(new DeviceSyncImpl(
          identity_manager, gcm_driver, connector, gcm_device_info_provider,
          std::move(url_loader_factory), simple_test_clock_,
          std::move(fake_pref_connection_delegate_), std::move(mock_timer_)));
    }

   private:
    std::unique_ptr<FakePrefConnectionDelegate> fake_pref_connection_delegate_;
    std::unique_ptr<base::MockOneShotTimer> mock_timer_;
    base::SimpleTestClock* simple_test_clock_;
  };

  DeviceSyncServiceTest()
      : test_devices_(GenerateTestRemoteDevices()),
        test_device_infos_(GenerateTestExternalDeviceInfos(test_devices_)),
        test_ineligible_devices_(
            GenerateTestIneligibleDevices(test_device_infos_)) {}
  ~DeviceSyncServiceTest() override = default;

  void SetUp() override {
    DBusThreadManager::Initialize();

    fake_gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();

    auto test_pref_service = std::make_unique<TestingPrefServiceSimple>();
    test_pref_service_ = test_pref_service.get();

    simple_test_clock_ = std::make_unique<base::SimpleTestClock>();

    // Note: The primary account is guaranteed to be available when the service
    //       starts up since this is a CrOS-only service, and CrOS requires that
    //       the user logs in.
    identity_test_environment_ =
        std::make_unique<identity::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(kTestEmail);

    fake_cryptauth_gcm_manager_factory_ =
        std::make_unique<FakeCryptAuthGCMManagerFactory>(fake_gcm_driver_.get(),
                                                         test_pref_service_);
    cryptauth::CryptAuthGCMManagerImpl::Factory::SetInstanceForTesting(
        fake_cryptauth_gcm_manager_factory_.get());

    fake_cryptauth_device_manager_factory_ =
        std::make_unique<FakeCryptAuthDeviceManagerFactory>(
            simple_test_clock_.get(), fake_cryptauth_gcm_manager_factory_.get(),
            test_pref_service_);
    cryptauth::CryptAuthDeviceManagerImpl::Factory::SetInstanceForTesting(
        fake_cryptauth_device_manager_factory_.get());

    fake_cryptauth_enrollment_manager_factory_ =
        std::make_unique<FakeCryptAuthEnrollmentManagerFactory>(
            simple_test_clock_.get(), fake_cryptauth_gcm_manager_factory_.get(),
            test_pref_service_);
    cryptauth::CryptAuthEnrollmentManagerImpl::Factory::SetInstanceForTesting(
        fake_cryptauth_enrollment_manager_factory_.get());

    fake_remote_device_provider_factory_ =
        std::make_unique<FakeRemoteDeviceProviderFactory>(
            test_devices_, identity_test_environment_->identity_manager(),
            fake_cryptauth_device_manager_factory_.get(),
            fake_cryptauth_enrollment_manager_factory_.get());
    cryptauth::RemoteDeviceProviderImpl::Factory::SetInstanceForTesting(
        fake_remote_device_provider_factory_.get());

    fake_software_feature_manager_factory_ =
        std::make_unique<FakeSoftwareFeatureManagerFactory>();
    cryptauth::SoftwareFeatureManagerImpl::Factory::SetInstanceForTesting(
        fake_software_feature_manager_factory_.get());

    auto fake_pref_connection_delegate =
        std::make_unique<FakePrefConnectionDelegate>(
            std::move(test_pref_service));
    fake_pref_connection_delegate_ = fake_pref_connection_delegate.get();

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    fake_device_sync_impl_factory_ =
        std::make_unique<FakeDeviceSyncImplFactory>(
            std::move(fake_pref_connection_delegate), std::move(mock_timer),
            simple_test_clock_.get());
    DeviceSyncImpl::Factory::SetInstanceForTesting(
        fake_device_sync_impl_factory_.get());

    fake_gcm_device_info_provider_ =
        std::make_unique<cryptauth::FakeGcmDeviceInfoProvider>(
            GetTestGcmDeviceInfo());

    auto shared_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));

    fake_device_sync_observer_ = std::make_unique<FakeDeviceSyncObserver>();
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::make_unique<DeviceSyncService>(
                identity_test_environment_->identity_manager(),
                fake_gcm_driver_.get(), fake_gcm_device_info_provider_.get(),
                shared_url_loader_factory));
  }

  void TearDown() override { DBusThreadManager::Shutdown(); }

  void ConnectToDeviceSyncService(bool device_already_enrolled_in_cryptauth) {
    // Used in CompleteConnectionToPrefService().
    device_already_enrolled_in_cryptauth_ =
        device_already_enrolled_in_cryptauth;

    fake_cryptauth_enrollment_manager_factory_
        ->set_device_already_enrolled_in_cryptauth(
            device_already_enrolled_in_cryptauth);

    // Must not have already connected.
    EXPECT_FALSE(connector_);

    // Create the Connector and bind it to |device_sync_|.
    connector_ = connector_factory_->CreateConnector();
    connector_->BindInterface(mojom::kServiceName, &device_sync_);

    // Set |fake_device_sync_observer_|.
    CallAddObserver();
  }

  void CompleteConnectionToPrefService() {
    EXPECT_TRUE(fake_pref_connection_delegate()->HasStartedPrefConnection());
    EXPECT_FALSE(fake_pref_connection_delegate()->HasFinishedPrefConnection());

    fake_pref_connection_delegate_->InvokePendingCallback();
    EXPECT_TRUE(fake_pref_connection_delegate()->HasFinishedPrefConnection());

    // When connection to preferences is complete, CryptAuth classes are
    // expected to be created and initialized.
    EXPECT_TRUE(fake_cryptauth_gcm_manager_factory_->instance()
                    ->has_started_listening());
    EXPECT_TRUE(
        fake_cryptauth_enrollment_manager_factory_->instance()->has_started());

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
    // CryptAuthDeviceManager::Start() is called as the last step of the
    // initialization flow.
    EXPECT_EQ(
        expected_to_be_initialized,
        fake_cryptauth_device_manager_factory_->instance()->has_started());
  }

  // Simulates an enrollment with success == |success|. If enrollment was not
  // yet in progress before this call, it is started before it is completed.
  void SimulateEnrollment(bool success) {
    cryptauth::FakeCryptAuthEnrollmentManager* enrollment_manager =
        fake_cryptauth_enrollment_manager_factory_->instance();

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
                    const cryptauth::RemoteDeviceList& updated_devices =
                        cryptauth::RemoteDeviceList()) {
    cryptauth::FakeCryptAuthDeviceManager* device_manager =
        fake_cryptauth_device_manager_factory_->instance();
    cryptauth::FakeRemoteDeviceProvider* remote_device_provider =
        fake_remote_device_provider_factory_->instance();

    EXPECT_TRUE(device_manager->IsSyncInProgress());
    device_manager->FinishActiveSync(
        success ? cryptauth::CryptAuthDeviceManager::SyncResult::SUCCESS
                : cryptauth::CryptAuthDeviceManager::SyncResult::FAILURE,
        updated_devices.empty()
            ? cryptauth::CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED
            : cryptauth::CryptAuthDeviceManager::DeviceChangeResult::CHANGED);

    if (!updated_devices.empty()) {
      remote_device_provider->set_synced_remote_devices(updated_devices);
      remote_device_provider->NotifyObserversDeviceListChanged();
    }
  }

  void InitializeServiceSuccessfully() {
    ConnectToDeviceSyncService(true /* device_already_enrolled_in_cryptauth */);
    CompleteConnectionToPrefService();
    VerifyInitializationStatus(true /* expected_to_be_initialized */);

    base::RunLoop().RunUntilIdle();

    // Enrollment did not occur since the device was already in a valid state.
    EXPECT_EQ(0u, fake_device_sync_observer()->num_enrollment_events());

    // The initial set of synced devices was set.
    EXPECT_EQ(1u, fake_device_sync_observer()->num_sync_events());
  }

  const cryptauth::RemoteDeviceList& test_devices() { return test_devices_; }

  const std::vector<cryptauth::ExternalDeviceInfo>& test_device_infos() {
    return test_device_infos_;
  }

  const std::vector<cryptauth::IneligibleDevice>& test_ineligible_devices() {
    return test_ineligible_devices_;
  }

  FakeDeviceSyncObserver* fake_device_sync_observer() {
    return fake_device_sync_observer_.get();
  }

  FakePrefConnectionDelegate* fake_pref_connection_delegate() {
    return fake_pref_connection_delegate_;
  }

  base::MockOneShotTimer* mock_timer() { return mock_timer_; }

  cryptauth::FakeCryptAuthEnrollmentManager*
  fake_cryptauth_enrollment_manager() {
    return fake_cryptauth_enrollment_manager_factory_->instance();
  }

  cryptauth::FakeCryptAuthDeviceManager* fake_cryptauth_device_manager() {
    return fake_cryptauth_device_manager_factory_->instance();
  }

  cryptauth::FakeSoftwareFeatureManager* fake_software_feature_manager() {
    return fake_software_feature_manager_factory_->instance();
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

    // SetSoftwareFeatureState() should return a struct with the special
    // kErrorNotInitialized error code.
    CallSetSoftwareFeatureState(
        test_devices()[0].public_key,
        cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST, true /* enabled */,
        true /* is_exclusive */);
    auto last_set_response = GetLastSetSoftwareFeatureStateResponseAndReset();
    EXPECT_EQ(mojom::NetworkRequestResult::kServiceNotYetInitialized,
              *last_set_response);

    // Likewise, FindEligibleDevices() should also return a struct with the same
    // error code.
    CallFindEligibleDevices(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);
    auto last_find_response = GetLastFindEligibleDevicesResponseAndReset();
    EXPECT_EQ(mojom::NetworkRequestResult::kServiceNotYetInitialized,
              last_find_response->first);
    EXPECT_FALSE(last_find_response->second /* response */);

    // GetDebugInfo() returns a null DebugInfo before initialization.
    EXPECT_FALSE(CallGetDebugInfo());
  }

  void CallAddObserver() {
    base::RunLoop run_loop;
    device_sync_->AddObserver(
        fake_device_sync_observer_->GenerateInterfacePtr(),
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

    if (fake_cryptauth_enrollment_manager_factory_->instance()) {
      EXPECT_EQ(last_force_enrollment_now_result_,
                fake_cryptauth_enrollment_manager_factory_->instance()
                    ->IsEnrollmentInProgress());
    }

    return last_force_enrollment_now_result_;
  }

  bool CallForceSyncNow() {
    base::RunLoop run_loop;
    device_sync_->ForceSyncNow(
        base::BindOnce(&DeviceSyncServiceTest::OnForceSyncNowCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    if (fake_cryptauth_device_manager_factory_->instance()) {
      EXPECT_EQ(last_force_sync_now_result_,
                fake_cryptauth_device_manager_factory_->instance()
                    ->IsSyncInProgress());
    }

    return last_force_sync_now_result_;
  }

  const base::Optional<cryptauth::RemoteDevice>& CallGetLocalDeviceMetadata() {
    base::RunLoop run_loop;
    device_sync_->GetLocalDeviceMetadata(base::BindOnce(
        &DeviceSyncServiceTest::OnGetLocalDeviceMetadataCompleted,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return last_local_device_metadata_result_;
  }

  const base::Optional<cryptauth::RemoteDeviceList>& CallGetSyncedDevices() {
    base::RunLoop run_loop;
    device_sync_->GetSyncedDevices(
        base::BindOnce(&DeviceSyncServiceTest::OnGetSyncedDevicesCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return last_synced_devices_result_;
  }

  void CallSetSoftwareFeatureState(const std::string& public_key,
                                   cryptauth::SoftwareFeature software_feature,
                                   bool enabled,
                                   bool is_exclusive) {
    base::RunLoop run_loop;
    cryptauth::FakeSoftwareFeatureManager* manager =
        fake_software_feature_manager();

    // If the manager has not yet been created, the service has not been
    // initialized. SetSoftwareFeatureState() is expected to respond
    // synchronously with an error.
    if (!manager) {
      device_sync_->SetSoftwareFeatureState(
          public_key, software_feature, enabled, is_exclusive,
          base::Bind(&DeviceSyncServiceTest::
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
        base::Bind(&DeviceSyncServiceTest::OnSetSoftwareFeatureStateCompleted,
                   base::Unretained(this)));
    run_loop.Run();

    fake_software_feature_manager_factory_->instance()->set_delegate(nullptr);
  }

  void CallFindEligibleDevices(cryptauth::SoftwareFeature software_feature) {
    base::RunLoop run_loop;
    cryptauth::FakeSoftwareFeatureManager* manager =
        fake_software_feature_manager();

    // If the manager has not yet been created, the service has not been
    // initialized. FindEligibleDevices() is expected to respond synchronously
    // with an error.
    if (!manager) {
      device_sync_->FindEligibleDevices(
          software_feature,
          base::Bind(&DeviceSyncServiceTest::
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
        base::Bind(&DeviceSyncServiceTest::OnFindEligibleDevicesCompleted,
                   base::Unretained(this)));
    run_loop.Run();

    fake_software_feature_manager_factory_->instance()->set_delegate(nullptr);
  }

  const base::Optional<mojom::DebugInfo>& CallGetDebugInfo() {
    base::RunLoop run_loop;
    device_sync_->GetDebugInfo(
        base::BindOnce(&DeviceSyncServiceTest::OnGetDebugInfoCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return last_debug_info_result_;
  }

 private:
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

  void OnGetLocalDeviceMetadataCompleted(
      base::OnceClosure quit_closure,
      const base::Optional<cryptauth::RemoteDevice>& local_device_metadata) {
    last_local_device_metadata_result_ = local_device_metadata;
    std::move(quit_closure).Run();
  }

  void OnGetSyncedDevicesCompleted(
      base::OnceClosure quit_closure,
      const base::Optional<cryptauth::RemoteDeviceList>& synced_devices) {
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

  void OnFindEligibleDevicesCompleted(
      mojom::NetworkRequestResult result_code,
      mojom::FindEligibleDevicesResponsePtr response) {
    EXPECT_FALSE(last_find_eligible_devices_response_);
    last_find_eligible_devices_response_ =
        std::make_unique<std::pair<mojom::NetworkRequestResult,
                                   mojom::FindEligibleDevicesResponsePtr>>(
            result_code, std::move(response));
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

  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const cryptauth::RemoteDeviceList test_devices_;
  const std::vector<cryptauth::ExternalDeviceInfo> test_device_infos_;
  const std::vector<cryptauth::IneligibleDevice> test_ineligible_devices_;

  TestingPrefServiceSimple* test_pref_service_;
  FakePrefConnectionDelegate* fake_pref_connection_delegate_;
  base::MockOneShotTimer* mock_timer_;
  std::unique_ptr<base::SimpleTestClock> simple_test_clock_;
  std::unique_ptr<FakeDeviceSyncImplFactory> fake_device_sync_impl_factory_;
  std::unique_ptr<FakeCryptAuthGCMManagerFactory>
      fake_cryptauth_gcm_manager_factory_;
  std::unique_ptr<FakeCryptAuthDeviceManagerFactory>
      fake_cryptauth_device_manager_factory_;
  std::unique_ptr<FakeCryptAuthEnrollmentManagerFactory>
      fake_cryptauth_enrollment_manager_factory_;
  std::unique_ptr<FakeRemoteDeviceProviderFactory>
      fake_remote_device_provider_factory_;
  std::unique_ptr<FakeSoftwareFeatureManagerFactory>
      fake_software_feature_manager_factory_;

  std::unique_ptr<identity::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<gcm::FakeGCMDriver> fake_gcm_driver_;
  std::unique_ptr<cryptauth::FakeGcmDeviceInfoProvider>
      fake_gcm_device_info_provider_;

  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;

  bool device_already_enrolled_in_cryptauth_;
  bool last_force_enrollment_now_result_;
  bool last_force_sync_now_result_;
  base::Optional<cryptauth::RemoteDeviceList> last_synced_devices_result_;
  base::Optional<cryptauth::RemoteDevice> last_local_device_metadata_result_;
  std::unique_ptr<mojom::NetworkRequestResult>
      last_set_software_feature_state_response_;
  std::unique_ptr<std::pair<mojom::NetworkRequestResult,
                            mojom::FindEligibleDevicesResponsePtr>>
      last_find_eligible_devices_response_;
  base::Optional<mojom::DebugInfo> last_debug_info_result_;

  std::unique_ptr<FakeDeviceSyncObserver> fake_device_sync_observer_;
  mojom::DeviceSyncPtr device_sync_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncServiceTest);
};

TEST_F(DeviceSyncServiceTest, PreferencesNeverConnect) {
  ConnectToDeviceSyncService(false /* device_already_enrolled_in_cryptauth */);

  // A connection to the Preferences service should have started.
  EXPECT_TRUE(fake_pref_connection_delegate()->HasStartedPrefConnection());
  EXPECT_FALSE(fake_pref_connection_delegate()->HasFinishedPrefConnection());

  // Do not complete the connection; without this step, the other API functions
  // should fail.
  VerifyApiFunctionsFailBeforeInitialization();

  // No observer callbacks should have been invoked.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, fake_device_sync_observer()->num_enrollment_events());
  EXPECT_EQ(0u, fake_device_sync_observer()->num_sync_events());
}

TEST_F(DeviceSyncServiceTest,
       DeviceNotAlreadyEnrolledInCryptAuth_FailsEnrollment) {
  ConnectToDeviceSyncService(false /* device_already_enrolled_in_cryptauth */);
  CompleteConnectionToPrefService();

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
  ConnectToDeviceSyncService(false /* device_already_enrolled_in_cryptauth */);
  CompleteConnectionToPrefService();

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
  cryptauth::RemoteDeviceList updated_device_list(test_devices().begin() + 1,
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
  InitializeServiceSuccessfully();

  const auto& set_software_calls =
      fake_software_feature_manager()->set_software_feature_state_calls();
  EXPECT_EQ(0u, set_software_calls.size());

  // Set the BETTER_TOGETHER_HOST field to "supported".
  cryptauth::RemoteDevice device_for_test = test_devices()[0];
  device_for_test
      .software_features[cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST] =
      cryptauth::SoftwareFeatureState::kSupported;
  EXPECT_TRUE(CallForceSyncNow());
  SimulateSync(true /* success */, {device_for_test});

  // Enable BETTER_TOGETHER_HOST for the device.
  CallSetSoftwareFeatureState(device_for_test.public_key,
                              cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
                              true /* enabled */, true /* is_exclusive */);
  EXPECT_EQ(1u, set_software_calls.size());
  EXPECT_EQ(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
            set_software_calls[0]->software_feature);
  EXPECT_TRUE(set_software_calls[0]->enabled);
  EXPECT_TRUE(set_software_calls[0]->is_exclusive);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastSetSoftwareFeatureStateResponseAndReset());

  // Now, invoke the success callback.
  set_software_calls[0]->success_callback.Run();

  // The callback still has not yet been invoked, since a device sync has not
  // confirmed the feature state change yet.
  EXPECT_FALSE(GetLastSetSoftwareFeatureStateResponseAndReset());

  // Simulate a sync which includes the device with the correct "enabled" state.
  device_for_test
      .software_features[cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST] =
      cryptauth::SoftwareFeatureState::kEnabled;
  base::RunLoop().RunUntilIdle();
  SimulateSync(true /* success */, {device_for_test});
  base::RunLoop().RunUntilIdle();

  auto last_response = GetLastSetSoftwareFeatureStateResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(device_sync::mojom::NetworkRequestResult::kSuccess, *last_response);
}

TEST_F(DeviceSyncServiceTest,
       SetSoftwareFeatureState_RequestSucceedsButDoesNotTakeEffect) {
  InitializeServiceSuccessfully();

  const auto& set_software_calls =
      fake_software_feature_manager()->set_software_feature_state_calls();
  EXPECT_EQ(0u, set_software_calls.size());

  // Set the BETTER_TOGETHER_HOST field to "supported".
  cryptauth::RemoteDevice device_for_test = test_devices()[0];
  device_for_test
      .software_features[cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST] =
      cryptauth::SoftwareFeatureState::kSupported;
  EXPECT_TRUE(CallForceSyncNow());
  SimulateSync(true /* success */, {device_for_test});

  // Enable BETTER_TOGETHER_HOST for the device.
  CallSetSoftwareFeatureState(device_for_test.public_key,
                              cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
                              true /* enabled */, true /* is_exclusive */);
  EXPECT_EQ(1u, set_software_calls.size());
  EXPECT_EQ(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
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
  EXPECT_EQ(device_sync::mojom::NetworkRequestResult::
                kRequestSucceededButUnexpectedResult,
            *last_response);
}

TEST_F(DeviceSyncServiceTest, SetSoftwareFeatureState_Error) {
  InitializeServiceSuccessfully();

  const auto& set_software_calls =
      fake_software_feature_manager()->set_software_feature_state_calls();
  EXPECT_EQ(0u, set_software_calls.size());

  // Set the BETTER_TOGETHER_HOST field to "supported".
  cryptauth::RemoteDevice device_for_test = test_devices()[0];
  device_for_test
      .software_features[cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST] =
      cryptauth::SoftwareFeatureState::kSupported;
  EXPECT_TRUE(CallForceSyncNow());
  SimulateSync(true /* success */, {device_for_test});

  // Enable BETTER_TOGETHER_HOST for the device.
  CallSetSoftwareFeatureState(device_for_test.public_key,
                              cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
                              true /* enabled */, true /* is_exclusive */);
  ASSERT_EQ(1u, set_software_calls.size());
  EXPECT_EQ(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
            set_software_calls[0]->software_feature);
  EXPECT_TRUE(set_software_calls[0]->enabled);
  EXPECT_TRUE(set_software_calls[0]->is_exclusive);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastSetSoftwareFeatureStateResponseAndReset());

  // Now, invoke the error callback.
  set_software_calls[0]->error_callback.Run(
      cryptauth::NetworkRequestError::kOffline);
  base::RunLoop().RunUntilIdle();
  auto last_response = GetLastSetSoftwareFeatureStateResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(mojom::NetworkRequestResult::kOffline, *last_response);
}

TEST_F(DeviceSyncServiceTest, FindEligibleDevices) {
  InitializeServiceSuccessfully();

  const auto& find_eligible_calls =
      fake_software_feature_manager()->find_eligible_multidevice_host_calls();
  EXPECT_EQ(0u, find_eligible_calls.size());

  // Find devices which are BETTER_TOGETHER_HOSTs.
  CallFindEligibleDevices(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);
  EXPECT_EQ(1u, find_eligible_calls.size());
  EXPECT_EQ(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
            find_eligible_calls[0]->software_feature);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastFindEligibleDevicesResponseAndReset());

  // Now, invoke the success callback, simultating that device 0 is eligible and
  // devices 1-4 are not.
  find_eligible_calls[0]->success_callback.Run(
      std::vector<cryptauth::ExternalDeviceInfo>(test_device_infos().begin(),
                                                 test_device_infos().begin()),
      std::vector<cryptauth::IneligibleDevice>(
          test_ineligible_devices().begin() + 1,
          test_ineligible_devices().end()));
  base::RunLoop().RunUntilIdle();
  auto last_response = GetLastFindEligibleDevicesResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(device_sync::mojom::NetworkRequestResult::kSuccess,
            last_response->first);
  EXPECT_EQ(last_response->second->eligible_devices,
            cryptauth::RemoteDeviceList(test_devices().begin(),
                                        test_devices().begin()));
  EXPECT_EQ(last_response->second->ineligible_devices,
            cryptauth::RemoteDeviceList(test_devices().begin() + 1,
                                        test_devices().end()));

  // Find devices which are BETTER_TOGETHER_HOSTs again.
  CallFindEligibleDevices(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);
  EXPECT_EQ(2u, find_eligible_calls.size());
  EXPECT_EQ(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
            find_eligible_calls[1]->software_feature);

  // The callback has not yet been invoked.
  EXPECT_FALSE(GetLastFindEligibleDevicesResponseAndReset());

  // Now, invoke the error callback.
  find_eligible_calls[1]->error_callback.Run(
      cryptauth::NetworkRequestError::kOffline);
  base::RunLoop().RunUntilIdle();
  last_response = GetLastFindEligibleDevicesResponseAndReset();
  EXPECT_TRUE(last_response);
  EXPECT_EQ(mojom::NetworkRequestResult::kOffline, last_response->first);
  EXPECT_FALSE(last_response->second /* response */);
}

TEST_F(DeviceSyncServiceTest, GetDebugInfo) {
  static const base::TimeDelta kTimeBetweenEpochAndLastEnrollment =
      base::TimeDelta::FromDays(365 * 50);  // 50 years
  static const base::TimeDelta kTimeUntilNextEnrollment =
      base::TimeDelta::FromDays(10);

  static const base::TimeDelta kTimeBetweenEpochAndLastSync =
      base::TimeDelta::FromDays(366 * 50);  // 50 years and 1 day
  static const base::TimeDelta kTimeUntilNextSync =
      base::TimeDelta::FromDays(11);

  InitializeServiceSuccessfully();

  fake_cryptauth_enrollment_manager()->set_last_enrollment_time(
      base::Time::FromDeltaSinceWindowsEpoch(
          kTimeBetweenEpochAndLastEnrollment));
  fake_cryptauth_enrollment_manager()->set_time_to_next_attempt(
      kTimeUntilNextEnrollment);
  fake_cryptauth_enrollment_manager()->set_is_recovering_from_failure(false);
  fake_cryptauth_enrollment_manager()->set_is_enrollment_in_progress(true);

  fake_cryptauth_device_manager()->set_last_sync_time(
      base::Time::FromDeltaSinceWindowsEpoch(kTimeBetweenEpochAndLastSync));
  fake_cryptauth_device_manager()->set_time_to_next_attempt(kTimeUntilNextSync);
  fake_cryptauth_device_manager()->set_is_recovering_from_failure(true);
  fake_cryptauth_device_manager()->set_is_sync_in_progress(false);

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
  EXPECT_TRUE(result->is_recovering_from_sync_failure);
  EXPECT_FALSE(result->is_sync_in_progress);
}

}  // namespace device_sync

}  // namespace chromeos
