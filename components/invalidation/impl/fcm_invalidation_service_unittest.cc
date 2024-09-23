// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/public/invalidation.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "fcm_sync_network_channel.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using instance_id::InstanceID;
using instance_id::InstanceIDDriver;
using testing::_;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace invalidation {

namespace {

constexpr char topic1[] = "user.policy";
constexpr char topic2[] = "device.policy";
constexpr char topic3[] = "remote_command";
constexpr char topic4[] = "drive";

template <class... Inv>
std::map<Topic, Invalidation> ExpectedInvalidations(Inv... inv) {
  std::map<Topic, Invalidation> expected_invalidations;
  (expected_invalidations.emplace(inv.topic(), inv), ...);
  return expected_invalidations;
}

class TestFCMSyncNetworkChannel : public FCMSyncNetworkChannel {
 public:
  void StartListening() override {}
  void StopListening() override {}
};

const char kApplicationName[] = "com.google.chrome.fcm.invalidations";
const char kSenderId[] = "invalidations-sender-id";

class MockInstanceID : public InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}

  MOCK_METHOD1(GetID, void(GetIDCallback callback));
  MOCK_METHOD1(GetCreationTime, void(GetCreationTimeCallback callback));
  MOCK_METHOD5(GetToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    base::TimeDelta time_to_live,
                    std::set<Flags> flags,
                    GetTokenCallback callback));
  MOCK_METHOD4(ValidateToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const std::string& token,
                    ValidateTokenCallback callback));

  MOCK_METHOD3(DeleteTokenImpl,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    DeleteTokenCallback callback));
  MOCK_METHOD1(DeleteIDImpl, void(DeleteIDCallback callback));
};

class MockInstanceIDDriver : public InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}

  MOCK_METHOD1(GetInstanceID, InstanceID*(const std::string& app_id));
  MOCK_METHOD1(RemoveInstanceID, void(const std::string& app_id));
  MOCK_CONST_METHOD1(ExistsInstanceID, bool(const std::string& app_id));
};

// A FakeInvalidationHandler that is "bound" to a specific
// InvalidationService.  This is for cross-referencing state information with
// the bound InvalidationService.
class BoundFakeInvalidationHandler : public FakeInvalidationHandler {
 public:
  BoundFakeInvalidationHandler(const InvalidationService& invalidator,
                               const std::string& owner)
      : FakeInvalidationHandler(owner), invalidator_(invalidator) {}

  BoundFakeInvalidationHandler(const BoundFakeInvalidationHandler&) = delete;
  BoundFakeInvalidationHandler& operator=(const BoundFakeInvalidationHandler&) =
      delete;

  // Returns the last return value of GetInvalidatorState() on the
  // bound invalidator from the last time the invalidator state
  // changed.
  InvalidatorState GetLastRetrievedState() const {
    return last_retrieved_state_;
  }

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(InvalidatorState state) override {
    FakeInvalidationHandler::OnInvalidatorStateChange(state);
    last_retrieved_state_ = invalidator_->GetInvalidatorState();
  }

 private:
  const raw_ref<const InvalidationService> invalidator_;
  InvalidatorState last_retrieved_state_ = InvalidatorState::kDisabled;
};

}  // namespace

class FCMInvalidationServiceTest : public testing::Test {
 public:
  FCMInvalidationServiceTest() {
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kInvalidationClientIDCache);
    InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
        pref_service_.registry());
    PerUserTopicSubscriptionManager::RegisterPrefs(pref_service_.registry());
  }

  void CreateInvalidationService() {
    CreateUninitializedInvalidationService();
    InitializeInvalidationService();
  }

  void CreateUninitializedInvalidationService() {
    gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();

    identity_test_env_.MakePrimaryAccountAvailable("example@gmail.com",
                                                   signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    identity_provider_ = std::make_unique<ProfileIdentityProvider>(
        identity_test_env_.identity_manager());

    mock_instance_id_driver_ =
        std::make_unique<testing::NiceMock<MockInstanceIDDriver>>();
    mock_instance_id_ = std::make_unique<testing::NiceMock<MockInstanceID>>();
    ON_CALL(*mock_instance_id_driver_,
            GetInstanceID(base::StrCat({kApplicationName, "-", kSenderId})))
        .WillByDefault(testing::Return(mock_instance_id_.get()));
    ON_CALL(*mock_instance_id_, GetID(_))
        .WillByDefault(testing::WithArg<0>(
            testing::Invoke([](InstanceID::GetIDCallback callback) {
              std::move(callback).Run("FakeIID");
            })));

    invalidation_service_ = std::make_unique<FCMInvalidationService>(
        identity_provider_.get(),
        base::BindRepeating(&FCMNetworkHandler::Create, gcm_driver_.get(),
                            mock_instance_id_driver_.get()),
        base::BindRepeating(
            [](raw_ptr<FCMInvalidationListener>& stored_listener,
               std::unique_ptr<FCMSyncNetworkChannel> channel) {
              auto listener = std::make_unique<FCMInvalidationListener>(
                  std::make_unique<TestFCMSyncNetworkChannel>());
              stored_listener = listener.get();

              return listener;
            },
            std::ref(listener_)),
        base::BindRepeating(&PerUserTopicSubscriptionManager::Create,
                            &url_loader_factory_),
        mock_instance_id_driver_.get(), &pref_service_, kSenderId);
  }

  bool IsInvalidationServiceStarted() {
    return invalidation_service_->IsStarted();
  }

  void InitializeInvalidationService() { invalidation_service_->Init(); }

  FCMInvalidationService* GetInvalidationService() {
    return invalidation_service_.get();
  }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidation_service_->OnInvalidatorStateChange(state);
  }

  template <class... TopicType>
  void TriggerSuccessfullySubscribed(TopicType... topics) {
    (invalidation_service_->OnSuccessfullySubscribed(topics), ...);
  }

  template <class... Inv>
  void TriggerOnIncomingInvalidation(Inv... inv) {
    (invalidation_service_->OnInvalidate(inv), ...);
  }

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<gcm::GCMDriver> gcm_driver_;
  std::unique_ptr<MockInstanceIDDriver> mock_instance_id_driver_;
  std::unique_ptr<MockInstanceID> mock_instance_id_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<IdentityProvider> identity_provider_;
  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple pref_service_;

  // The service has to be below the provider since the service keeps
  // a non-owned pointer to the provider.
  std::unique_ptr<FCMInvalidationService> invalidation_service_;
  raw_ptr<FCMInvalidationListener> listener_;  // Owned by the service.
};

// Initialize the invalidator, register a handler, register some IDs for that
// handler, and then unregister the handler, dispatching invalidations in
// between.  The handler should only see invalidations when its registered and
// its IDs are registered.
TEST_F(FCMInvalidationServiceTest, Basic) {
  CreateInvalidationService();
  InvalidationService* const invalidator = GetInvalidationService();

  FakeInvalidationHandler handler("owner");

  invalidator->AddObserver(&handler);

  const auto inv1 = Invalidation(topic1, 1, "1");
  const auto inv2 = Invalidation(topic2, 2, "2");
  const auto inv3 = Invalidation(topic3, 3, "3");

  // Should be ignored since no IDs are registered to |handler|.
  TriggerSuccessfullySubscribed(topic1, topic2, topic3);
  EXPECT_THAT(handler.GetSuccessfullySubscribed(), IsEmpty());
  TriggerOnIncomingInvalidation(inv1, inv2, inv3);
  EXPECT_EQ(0, handler.GetInvalidationCount());

  TopicSet topics;
  topics.insert(topic1);
  topics.insert(topic2);
  EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler, topics));

  TriggerOnInvalidatorStateChange(InvalidatorState::kEnabled);
  EXPECT_EQ(InvalidatorState::kEnabled, handler.GetInvalidatorState());

  TriggerSuccessfullySubscribed(topic1, topic2, topic3);
  EXPECT_THAT(handler.GetSuccessfullySubscribed(),
              UnorderedElementsAre(topic1, topic2));

  TriggerOnIncomingInvalidation(inv1, inv2, inv3);
  EXPECT_EQ(2, handler.GetInvalidationCount());
  EXPECT_EQ(ExpectedInvalidations(inv1, inv2),
            handler.GetReceivedInvalidations());
  handler.Clear();

  topics.erase(topic1);
  topics.insert(topic3);
  EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler, topics));

  // Removed Topics should not be notified, newly-added ones should.
  TriggerSuccessfullySubscribed(topic1, topic2, topic3);
  EXPECT_THAT(handler.GetSuccessfullySubscribed(),
              UnorderedElementsAre(topic2, topic3));

  TriggerOnIncomingInvalidation(inv1, inv2, inv3);
  EXPECT_EQ(2, handler.GetInvalidationCount());
  EXPECT_EQ(ExpectedInvalidations(inv2, inv3),
            handler.GetReceivedInvalidations());
  handler.Clear();

  TriggerOnInvalidatorStateChange(InvalidatorState::kDisabled);
  EXPECT_EQ(InvalidatorState::kDisabled, handler.GetInvalidatorState());

  TriggerOnInvalidatorStateChange(InvalidatorState::kEnabled);
  EXPECT_EQ(InvalidatorState::kEnabled, handler.GetInvalidatorState());

  invalidator->RemoveObserver(&handler);

  // Should be ignored since |handler| isn't registered anymore.
  TriggerSuccessfullySubscribed(topic1, topic2, topic3);
  EXPECT_THAT(handler.GetSuccessfullySubscribed(), IsEmpty());

  TriggerOnIncomingInvalidation(inv1, inv2, inv3);
  EXPECT_EQ(0, handler.GetInvalidationCount());
}

// Register handlers and some topics for those handlers, register a handler
// with no topics, and register a handler with some topics but unregister it.
// Then, dispatch some invalidations and invalidations.  Handlers that are
// registered should get invalidations, and the ones that have registered
// topics should receive invalidations for those topics.
TEST_F(FCMInvalidationServiceTest, MultipleHandlers) {
  CreateInvalidationService();
  InvalidationService* const invalidator = GetInvalidationService();

  FakeInvalidationHandler handler1(/*owner=*/"owner_1");
  FakeInvalidationHandler handler2(/*owner=*/"owner_2");
  FakeInvalidationHandler handler3(/*owner=*/"owner_3");
  FakeInvalidationHandler handler4(/*owner=*/"owner_4");

  invalidator->AddObserver(&handler1);
  invalidator->AddObserver(&handler2);
  invalidator->AddObserver(&handler3);
  invalidator->AddObserver(&handler4);

  {
    TopicSet topics;
    topics.insert(topic1);
    topics.insert(topic2);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler1, topics));
  }

  {
    TopicSet topics;
    topics.insert(topic3);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler2, topics));
  }

  // Don't register any topics for handler3.

  {
    TopicSet topics;
    topics.insert(topic4);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler4, topics));
  }

  invalidator->RemoveObserver(&handler4);

  TriggerOnInvalidatorStateChange(InvalidatorState::kEnabled);
  EXPECT_EQ(InvalidatorState::kEnabled, handler1.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kEnabled, handler2.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kEnabled, handler3.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kDisabled, handler4.GetInvalidatorState());

  TriggerSuccessfullySubscribed(topic1, topic2, topic3, topic4);
  EXPECT_THAT(handler1.GetSuccessfullySubscribed(),
              UnorderedElementsAre(topic1, topic2));
  EXPECT_THAT(handler2.GetSuccessfullySubscribed(),
              UnorderedElementsAre(topic3));
  EXPECT_THAT(handler3.GetSuccessfullySubscribed(), IsEmpty());
  EXPECT_THAT(handler4.GetSuccessfullySubscribed(), IsEmpty());

  {
    const auto inv1 = Invalidation(topic1, 1, "1");
    const auto inv2 = Invalidation(topic2, 2, "2");
    const auto inv3 = Invalidation(topic3, 3, "3");
    const auto inv4 = Invalidation(topic4, 4, "4");
    TriggerOnIncomingInvalidation(inv1, inv2, inv3, inv4);

    EXPECT_EQ(2, handler1.GetInvalidationCount());
    EXPECT_EQ(ExpectedInvalidations(inv1, inv2),
              handler1.GetReceivedInvalidations());

    EXPECT_EQ(1, handler2.GetInvalidationCount());
    EXPECT_EQ(ExpectedInvalidations(inv3), handler2.GetReceivedInvalidations());

    EXPECT_EQ(0, handler3.GetInvalidationCount());
    EXPECT_EQ(0, handler4.GetInvalidationCount());
  }

  TriggerOnInvalidatorStateChange(InvalidatorState::kDisabled);
  EXPECT_EQ(InvalidatorState::kDisabled, handler1.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kDisabled, handler2.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kDisabled, handler3.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kDisabled, handler4.GetInvalidatorState());

  invalidator->RemoveObserver(&handler3);
  invalidator->RemoveObserver(&handler2);
  invalidator->RemoveObserver(&handler1);
}

// Multiple registrations by different handlers on the same Topic should return
// false.
TEST_F(FCMInvalidationServiceTest, MultipleRegistrations) {
  CreateInvalidationService();
  InvalidationService* const invalidator = GetInvalidationService();

  FakeInvalidationHandler handler1(/*owner=*/"owner_1");
  FakeInvalidationHandler handler2(/*owner=*/"owner_2");

  invalidator->AddObserver(&handler1);
  invalidator->AddObserver(&handler2);

  // Registering both handlers for the same topic. First call should succeed,
  // second should fail.
  TopicSet topics;
  topics.insert(topic1);
  EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler1, topics));
  EXPECT_FALSE(invalidator->UpdateInterestedTopics(&handler2, topics));

  invalidator->RemoveObserver(&handler2);
  invalidator->RemoveObserver(&handler1);
}

// Make sure that passing an empty set to UpdateInterestedTopics clears
// the corresponding entries for the handler.
TEST_F(FCMInvalidationServiceTest, EmptySetUnregisters) {
  CreateInvalidationService();
  InvalidationService* const invalidator = GetInvalidationService();

  FakeInvalidationHandler handler1(/*owner=*/"owner_1");

  // Control observer.
  FakeInvalidationHandler handler2(/*owner=*/"owner_2");

  invalidator->AddObserver(&handler1);
  invalidator->AddObserver(&handler2);

  {
    TopicSet topics;
    topics.insert(topic1);
    topics.insert(topic2);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler1, topics));
  }

  {
    TopicSet topics;
    topics.insert(topic3);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler2, topics));
  }

  // Unregister the topics for the first observer. It should not receive any
  // further invalidations.
  EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler1, TopicSet()));

  TriggerOnInvalidatorStateChange(InvalidatorState::kEnabled);
  EXPECT_EQ(InvalidatorState::kEnabled, handler1.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kEnabled, handler2.GetInvalidatorState());

  TriggerSuccessfullySubscribed(topic1, topic2, topic3);
  EXPECT_THAT(handler1.GetSuccessfullySubscribed(), IsEmpty());
  EXPECT_THAT(handler2.GetSuccessfullySubscribed(),
              UnorderedElementsAre(topic3));

  {
    const auto inv1 = Invalidation(topic1, 1, "1");
    const auto inv2 = Invalidation(topic2, 2, "2");
    const auto inv3 = Invalidation(topic3, 3, "3");
    TriggerOnIncomingInvalidation(inv1, inv2, inv3);
    EXPECT_EQ(0, handler1.GetInvalidationCount());
    EXPECT_EQ(1, handler2.GetInvalidationCount());
  }

  TriggerOnInvalidatorStateChange(InvalidatorState::kDisabled);
  EXPECT_EQ(InvalidatorState::kDisabled, handler1.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kDisabled, handler2.GetInvalidatorState());

  invalidator->RemoveObserver(&handler2);
  invalidator->RemoveObserver(&handler1);
}

TEST_F(FCMInvalidationServiceTest, GetInvalidatorStateAlwaysCurrent) {
  CreateInvalidationService();
  InvalidationService* const invalidator = GetInvalidationService();

  BoundFakeInvalidationHandler handler(*invalidator, "owner");
  invalidator->AddObserver(&handler);

  TriggerOnInvalidatorStateChange(InvalidatorState::kEnabled);
  EXPECT_EQ(InvalidatorState::kEnabled, handler.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kEnabled, handler.GetLastRetrievedState());

  TriggerOnInvalidatorStateChange(InvalidatorState::kDisabled);
  EXPECT_EQ(InvalidatorState::kDisabled, handler.GetInvalidatorState());
  EXPECT_EQ(InvalidatorState::kDisabled, handler.GetLastRetrievedState());

  invalidator->RemoveObserver(&handler);
}

TEST_F(FCMInvalidationServiceTest, NotifiesAboutInstanceID) {
  // Set up a cached InstanceID aka client ID stored in prefs.
  {
    ScopedDictPrefUpdate update(&pref_service_,
                                prefs::kInvalidationClientIDCache);
    update->Set(kSenderId, "InstanceIDFromPrefs");
  }

  // Create the invalidation service, but do not initialize it yet.
  CreateUninitializedInvalidationService();
  FCMInvalidationService* invalidation_service = GetInvalidationService();
  ASSERT_TRUE(invalidation_service->GetInvalidatorClientId().empty());

  // Make sure the MockInstanceID doesn't immediately provide a fresh client ID.
  InstanceID::GetIDCallback get_id_callback;
  EXPECT_CALL(*mock_instance_id_, GetID(_))
      .WillOnce([&](InstanceID::GetIDCallback callback) {
        get_id_callback = std::move(callback);
      });

  // Initialize the service. It should read the client ID from prefs.
  InitializeInvalidationService();
  // The invalidation service has requested a fresh client ID.
  ASSERT_FALSE(get_id_callback.is_null());

  // The invalidation service should have restored the client ID from prefs.
  EXPECT_EQ(invalidation_service->GetInvalidatorClientId(),
            "InstanceIDFromPrefs");

  // Set another client ID in the invalidation service.
  std::move(get_id_callback).Run("FreshInstanceID");
  EXPECT_EQ(invalidation_service->GetInvalidatorClientId(), "FreshInstanceID");
}

TEST_F(FCMInvalidationServiceTest, ClearsInstanceIDOnSignout) {
  // Set up an invalidation service and make sure it generated a client ID (aka
  // InstanceID).
  CreateInvalidationService();
  FCMInvalidationService* invalidation_service = GetInvalidationService();
  ASSERT_FALSE(invalidation_service->GetInvalidatorClientId().empty());

  // Remove the active account (in practice, this means disabling
  // Sync-the-feature, or just signing out of the content are if only
  // Sync-the-transport was running). This should trigger deleting the
  // InstanceID.
  EXPECT_CALL(*mock_instance_id_, DeleteIDImpl(_));
  // Invalidation service owns the invalidation listener, and destroys it
  // OnActiveAccountLogout.
  // Resetting listener_ here, otherwise it causes dangling raw_ptr.
  listener_ = nullptr;
  invalidation_service->OnActiveAccountLogout();

  // Also the cached InstanceID (aka ClientID) in the invalidation service
  // should be gone. (Right now, the invalidation service clears its cache
  // immediately. In the future, it might be changed to first wait for the
  // asynchronous DeleteID operation to complete, in which case this test will
  // have to be updated.)
  EXPECT_TRUE(invalidation_service->GetInvalidatorClientId().empty());
}

TEST_F(FCMInvalidationServiceTest, ObserverBasics) {
  // Set up an invalidation service and make sure it generated a client ID (aka
  // InstanceID).
  CreateInvalidationService();
  FCMInvalidationService* invalidation_service = GetInvalidationService();

  FakeInvalidationHandler handler("some_name");
  EXPECT_FALSE(invalidation_service->HasObserver(&handler));
  invalidation_service->AddObserver(&handler);
  EXPECT_TRUE(invalidation_service->HasObserver(&handler));
  invalidation_service->RemoveObserver(&handler);
  EXPECT_FALSE(invalidation_service->HasObserver(&handler));
}

TEST_F(FCMInvalidationServiceTest, StartsIfNotDisabledWithSwitch) {
  // Create the invalidation service, but do not initialize it yet.
  CreateUninitializedInvalidationService();
  ASSERT_FALSE(IsInvalidationServiceStarted());

  // Initialize the service.
  InitializeInvalidationService();

  EXPECT_TRUE(IsInvalidationServiceStarted());
}

TEST_F(FCMInvalidationServiceTest, DoesNotStartIfDisabledWithSwitch) {
  // Set --disable-fcm-invalidations flag to disable invalidations.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "--disable-fcm-invalidations");
  // Create the invalidation service, but do not initialize it yet.
  CreateUninitializedInvalidationService();
  ASSERT_FALSE(IsInvalidationServiceStarted());

  // Initialize the service.
  InitializeInvalidationService();

  EXPECT_FALSE(IsInvalidationServiceStarted());
}

}  // namespace invalidation
