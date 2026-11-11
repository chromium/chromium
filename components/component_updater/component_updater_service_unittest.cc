// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

using Configurator = ::update_client::Configurator;
using Result = ::update_client::CrxInstaller::Result;
using TestConfigurator = ::update_client::TestConfigurator;
using UpdateClient = ::update_client::UpdateClient;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::Unused;

class MockInstaller : public update_client::CrxInstaller {
 public:
  MockInstaller() = default;
  MOCK_METHOD(void,
              Install,
              (const base::FilePath& unpack_path,
               const std::string& public_key,
               std::unique_ptr<InstallParams> install_params,
               ProgressCallback progress_callback,
               Callback callback),
              (override));
  MOCK_METHOD(std::optional<base::FilePath>,
              GetInstalledFile,
              (const std::string& file),
              (override));
  MOCK_METHOD(bool, Uninstall, (), (override));

 private:
  ~MockInstaller() override = default;
};

class MockUpdateClient : public UpdateClient {
 public:
  MockUpdateClient() = default;

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(base::RepeatingClosure,
              Install,
              (const std::string& id,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               Callback callback),
              (override));
  MOCK_METHOD(void,
              Update,
              (const std::vector<std::string>& ids,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               bool is_foreground,
               Callback callback),
              (override));
  MOCK_METHOD(void,
              CheckForUpdate,
              (const std::string& ids,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               bool is_foreground,
               Callback callback),
              (override));
  MOCK_METHOD(bool,
              GetCrxUpdateState,
              (const std::string& id, CrxUpdateItem* update_item),
              (const, override));
  MOCK_METHOD(bool, IsUpdating, (const std::string& id), (const, override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(bool, Cancel, (const std::string& id), (override));
  MOCK_METHOD(void,
              SendPing,
              (const CrxComponent& crx_component,
               PingParams ping_params,
               Callback callback),
              (override));
  MOCK_METHOD(void,
              CleanupStaleDownloads,
              (base::Time older_than, base::OnceClosure callback),
              (override));

 private:
  ~MockUpdateClient() override = default;
};

class MockServiceObserver : public ServiceObserver {
 public:
  MOCK_METHOD(void, OnEvent, (const update_client::CrxUpdateItem&), (override));
};

class MockUpdateScheduler : public UpdateScheduler {
 public:
  MOCK_METHOD(void,
              Schedule,
              (base::TimeDelta initial_delay,
               base::TimeDelta delay,
               const UserTask& user_task,
               const OnStopTaskCallback& on_stop),
              (override));
  MOCK_METHOD(void, Stop, (), (override));
};

class LoopHandler {
 public:
  explicit LoopHandler(int max_cnt, base::OnceClosure quit_closure)
      : max_cnt_(max_cnt), quit_closure_(std::move(quit_closure)) {}

  base::RepeatingClosure OnInstall(const std::string&,
                                   UpdateClient::CrxDataCallback,
                                   UpdateClient::CrxStateChangeCallback,
                                   Callback callback) {
    Handle(std::move(callback));
    return base::DoNothing();
  }

  void OnUpdate(const std::vector<std::string>&,
                UpdateClient::CrxDataCallback,
                UpdateClient::CrxStateChangeCallback,
                bool is_foreground,
                Callback callback) {
    Handle(std::move(callback));
  }

 private:
  void Handle(Callback callback) {
    ++cnt_;
    if (cnt_ >= max_cnt_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(quit_closure_));
    }
    std::move(callback).Run(update_client::Error::NONE);
  }

  const int max_cnt_ = 0;
  base::OnceClosure quit_closure_;
  int cnt_ = 0;
};

class ComponentUpdaterTest : public testing::Test {
 public:
  ComponentUpdaterTest();
  ComponentUpdaterTest(const ComponentUpdaterTest&) = delete;
  ComponentUpdaterTest& operator=(const ComponentUpdaterTest&) = delete;
  ~ComponentUpdaterTest() override;

  // Makes the full path to a component updater test file.
  const base::FilePath test_file(const char* file);

  MockUpdateClient& update_client() { return *update_client_; }
  ComponentUpdateService& component_updater() { return *component_updater_; }
  scoped_refptr<TestConfigurator> configurator() const { return config_; }
  base::OnceClosure quit_closure() { return runloop_.QuitClosure(); }
  MockUpdateScheduler& scheduler() { return *scheduler_; }

 protected:
  // `time_source` selects the time source of the task environment; the metered
  // connection tests use mock time to run the delayed resumption of updates.
  explicit ComponentUpdaterTest(
      base::test::TaskEnvironment::TimeSource time_source);

  void RunThreads();
  void TriggerUpdateComplete(const std::string& id);

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  void RunUpdateTask(const UpdateScheduler::UserTask& user_task);
  void Schedule(base::TimeDelta initial_delay,
                base::TimeDelta delay,
                const UpdateScheduler::UserTask& user_task,
                const UpdateScheduler::OnStopTaskCallback& on_stop);

  base::test::TaskEnvironment task_environment_;
  base::RunLoop runloop_;

  std::unique_ptr<TestingPrefServiceSimple> pref_ =
      std::make_unique<TestingPrefServiceSimple>();
  scoped_refptr<TestConfigurator> config_;
  scoped_refptr<MockUpdateClient> update_client_ =
      base::MakeRefCounted<MockUpdateClient>();
  std::unique_ptr<ComponentUpdateService> component_updater_;
  raw_ptr<MockUpdateScheduler> scheduler_;
};

class OnDemandTester {
 public:
  void OnDemand(ComponentUpdateService* cus,
                const std::string& id,
                OnDemandUpdater::Priority priority);
  void Wait() { run_loop_.Run(); }
  update_client::Error error() const { return error_; }

 private:
  void OnDemandComplete(update_client::Error error);

  base::RunLoop run_loop_;
  update_client::Error error_ = update_client::Error::NONE;
};

void OnDemandTester::OnDemand(ComponentUpdateService* cus,
                              const std::string& id,
                              OnDemandUpdater::Priority priority) {
  cus->GetOnDemandUpdater().OnDemandUpdate(
      id, priority,
      base::BindOnce(&OnDemandTester::OnDemandComplete,
                     base::Unretained(this)));
}

void OnDemandTester::OnDemandComplete(update_client::Error error) {
  error_ = error;
  run_loop_.Quit();
}

std::unique_ptr<ComponentUpdateService> TestComponentUpdateServiceFactory(
    scoped_refptr<Configurator> config) {
  EXPECT_TRUE(config);
  return std::make_unique<CrxUpdateService>(
      config, std::make_unique<MockUpdateScheduler>(),
      base::MakeRefCounted<MockUpdateClient>(), "",
      /*update_checks_disabled=*/false);
}

ComponentUpdaterTest::ComponentUpdaterTest()
    : ComponentUpdaterTest(base::test::TaskEnvironment::TimeSource::DEFAULT) {}

ComponentUpdaterTest::ComponentUpdaterTest(
    base::test::TaskEnvironment::TimeSource time_source)
    : task_environment_(time_source) {
  EXPECT_CALL(update_client(), AddObserver(_));
  auto scheduler = std::make_unique<MockUpdateScheduler>();
  scheduler_ = scheduler.get();
  ON_CALL(*scheduler_, Schedule(_, _, _, _))
      .WillByDefault(Invoke(this, &ComponentUpdaterTest::Schedule));
  RegisterComponentUpdateServicePrefs(pref_->registry());
  update_client::RegisterPrefs(pref_->registry());
  config_ = base::MakeRefCounted<TestConfigurator>(pref_.get());
  component_updater_ = std::make_unique<CrxUpdateService>(
      config_, std::move(scheduler), update_client_, "",
      /*update_checks_disabled=*/false);
}

ComponentUpdaterTest::~ComponentUpdaterTest() {
  EXPECT_CALL(update_client(), RemoveObserver(_));
}

void ComponentUpdaterTest::RunThreads() {
  runloop_.Run();
}

void ComponentUpdaterTest::TriggerUpdateComplete(const std::string& id) {
  update_client::Callback update_callback;
  EXPECT_CALL(update_client(), Update)
      .WillOnce([&update_callback](const std::vector<std::string>&,
                                   UpdateClient::CrxDataCallback,
                                   UpdateClient::CrxStateChangeCallback, bool,
                                   update_client::Callback callback) {
        update_callback = std::move(callback);
      });

  OnDemandTester tester;
  tester.OnDemand(&component_updater(), id,
                  OnDemandUpdater::Priority::FOREGROUND);

  ASSERT_TRUE(update_callback);
  std::move(update_callback).Run(update_client::Error::NONE);
}

void ComponentUpdaterTest::RunUpdateTask(
    const UpdateScheduler::UserTask& user_task) {
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindRepeating(
                     [](const UpdateScheduler::UserTask& user_task,
                        ComponentUpdaterTest* test) {
                       user_task.Run(base::BindOnce(
                           [](const UpdateScheduler::UserTask& user_task,
                              ComponentUpdaterTest* test) {
                             test->RunUpdateTask(user_task);
                           },
                           user_task, base::Unretained(test)));
                     },
                     user_task, base::Unretained(this)));
}

void ComponentUpdaterTest::Schedule(
    base::TimeDelta initial_delay,
    base::TimeDelta delay,
    const UpdateScheduler::UserTask& user_task,
    const UpdateScheduler::OnStopTaskCallback& on_stop) {
  RunUpdateTask(user_task);
}

TEST_F(ComponentUpdaterTest, AddObserver) {
  MockServiceObserver observer;
  EXPECT_CALL(update_client(), AddObserver(&observer));
  EXPECT_CALL(update_client(), Stop());
  EXPECT_CALL(scheduler(), Stop());
  component_updater().AddObserver(&observer);
}

TEST_F(ComponentUpdaterTest, RemoveObserver) {
  MockServiceObserver observer;
  EXPECT_CALL(update_client(), RemoveObserver(&observer));
  EXPECT_CALL(update_client(), Stop());
  EXPECT_CALL(scheduler(), Stop());
  component_updater().RemoveObserver(&observer);
}

// Tests that UpdateClient::Update is called by the timer loop when
// components are registered, and the component update starts.
// Also tests that Uninstall is called when a component is unregistered.
TEST_F(ComponentUpdaterTest, RegisterComponent) {
  base::HistogramTester ht;

  scoped_refptr<MockInstaller> installer =
      base::MakeRefCounted<MockInstaller>();
  EXPECT_CALL(*installer, Uninstall()).WillOnce(Return(true));

  const std::string id1 = "abagagagagagagagagagagagagagagag";
  const std::string id2 = "jebgalgnebhfojomionfpkfelancnnkf";

  // Quit after two update checks have fired.
  LoopHandler loop_handler(2, quit_closure());
  EXPECT_CALL(update_client(), Update(_, _, _, /*is_foreground=*/false, _))
      .WillRepeatedly(Invoke(&loop_handler, &LoopHandler::OnUpdate));

  EXPECT_CALL(update_client(), IsUpdating(id1));
  EXPECT_CALL(update_client(), Stop());
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _));
  EXPECT_CALL(scheduler(), Stop());

  EXPECT_TRUE(component_updater().RegisterComponent(
      {.app_id = id1,
       .public_key_hash = base::ToVector(update_client::abag_hash),
       .version = base::Version("1.0"),
       .installer = installer,
       .requires_network_encryption = false,
       .supports_group_policy_enable_component_updates = true}));
  EXPECT_TRUE(component_updater().RegisterComponent(
      {.app_id = id2,
       .public_key_hash = base::ToVector(update_client::jebg_hash),
       .version = base::Version("0.9"),
       .installer = installer,
       .requires_network_encryption = false,
       .supports_group_policy_enable_component_updates = true}));

  RunThreads();
  EXPECT_TRUE(component_updater().UnregisterComponent(id1));

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 1, 2);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 2);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 2);
}

// Tests that on-demand updates invoke UpdateClient::Update.
TEST_F(ComponentUpdaterTest, OnDemandUpdate) {
  base::HistogramTester ht;

  // Don't run periodic update task.
  ON_CALL(scheduler(), Schedule(_, _, _, _)).WillByDefault(Return());

  auto& cus = component_updater();

  // Tests calling OnDemand for an unregistered component. This call results in
  // an error, which is recorded by the OnDemandTester instance. Since the
  // component was not registered, the call is ignored for UMA metrics.
  OnDemandTester ondemand_tester_component_not_registered;
  ondemand_tester_component_not_registered.OnDemand(
      &cus, "ihfokbkgjpifnbbojhneepfflplebdkc",
      OnDemandUpdater::Priority::FOREGROUND);

  // Register two components, then call `OnDemand` for each component, with
  // foreground and background priorities. Expect calls to `Schedule` because
  // components have registered, calls `Update` corresponding to each `OnDemand`
  // invocation, and calls to `Stop` when the mocks are torn down.
  LoopHandler loop_handler(2, quit_closure());
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _));
  EXPECT_CALL(update_client(), Update(_, _, _, /*is_foreground=*/true, _))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnUpdate));
  EXPECT_CALL(update_client(), Update(_, _, _, /*is_foreground=*/false, _))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnUpdate));
  EXPECT_CALL(update_client(), Stop());
  EXPECT_CALL(scheduler(), Stop());

  {
    EXPECT_TRUE(cus.RegisterComponent(
        {.app_id = "jebgalgnebhfojomionfpkfelancnnkf",
         .public_key_hash = base::ToVector(update_client::jebg_hash),
         .version = base::Version("0.9"),
         .installer = base::MakeRefCounted<MockInstaller>(),
         .requires_network_encryption = false,
         .supports_group_policy_enable_component_updates = true}));
  }
  {
    EXPECT_TRUE(cus.RegisterComponent(
        {.app_id = "abagagagagagagagagagagagagagagag",
         .public_key_hash = base::ToVector(update_client::abag_hash),
         .version = base::Version("0.9"),
         .installer = base::MakeRefCounted<MockInstaller>(),
         .requires_network_encryption = false,
         .supports_group_policy_enable_component_updates = true}));
  }

  OnDemandTester ondemand_tester;
  ondemand_tester.OnDemand(&cus, "jebgalgnebhfojomionfpkfelancnnkf",
                           OnDemandUpdater::Priority::FOREGROUND);
  ondemand_tester.OnDemand(&cus, "abagagagagagagagagagagagagagagag",
                           OnDemandUpdater::Priority::BACKGROUND);
  RunThreads();

  EXPECT_EQ(update_client::Error::INVALID_ARGUMENT,
            ondemand_tester_component_not_registered.error());
  EXPECT_EQ(update_client::Error::NONE, ondemand_tester.error());

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 0, 2);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 2);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 2);
}

// Tests that with update checks disabled, components are registered but no
// update checks are made, neither periodically nor on demand.
TEST_F(ComponentUpdaterTest, UpdateChecksDisabled) {
  auto update_client = base::MakeRefCounted<MockUpdateClient>();
  auto scheduler = std::make_unique<MockUpdateScheduler>();
  EXPECT_CALL(*update_client, AddObserver(_));
  EXPECT_CALL(*update_client, RemoveObserver(_));
  EXPECT_CALL(*update_client, Stop());
  EXPECT_CALL(*update_client, Update).Times(0);
  EXPECT_CALL(*scheduler, Schedule).Times(0);
  EXPECT_CALL(*scheduler, Stop());
  CrxUpdateService cus(configurator(), std::move(scheduler), update_client, "",
                       /*update_checks_disabled=*/true);

  const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
  EXPECT_TRUE(cus.RegisterComponent(
      {.app_id = id,
       .public_key_hash = base::ToVector(update_client::jebg_hash),
       .version = base::Version("0.9"),
       .installer = base::MakeRefCounted<MockInstaller>(),
       .requires_network_encryption = false,
       .supports_group_policy_enable_component_updates = true}));
  CrxUpdateItem item;
  EXPECT_TRUE(cus.GetComponentDetails(id, &item));

  base::test::TestFuture<update_client::Error> future;
  cus.OnDemandUpdate(id, OnDemandUpdater::Priority::FOREGROUND,
                     future.GetCallback());
  EXPECT_EQ(future.Get(), update_client::Error::UPDATE_DISABLED);

  // Also for components that are not registered, which is the case for the
  // components normally registered by RegisterComponentsForUpdate().
  base::test::TestFuture<update_client::Error> unregistered_future;
  cus.OnDemandUpdate("ihfokbkgjpifnbbojhneepfflplebdkc",
                     OnDemandUpdater::Priority::FOREGROUND,
                     unregistered_future.GetCallback());
  EXPECT_EQ(unregistered_future.Get(), update_client::Error::UPDATE_DISABLED);

  // Throttled requests are unblocked right away instead of waiting on an
  // update.
  bool unblocked = false;
  cus.MaybeThrottle(
      id,
      base::BindOnce([](bool* unblocked) { *unblocked = true; }, &unblocked));
  EXPECT_TRUE(unblocked);
}

// Tests that throttling an update invokes UpdateClient::Update.
TEST_F(ComponentUpdaterTest, MaybeThrottle) {
  base::HistogramTester ht;

  // Don't run periodic update task.
  ON_CALL(scheduler(), Schedule(_, _, _, _)).WillByDefault(Return());

  LoopHandler loop_handler(1, quit_closure());
  EXPECT_CALL(update_client(), Update(_, _, _, /*is_foreground=*/true, _))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnUpdate));
  EXPECT_CALL(update_client(), Stop());
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _));
  EXPECT_CALL(scheduler(), Stop());

  EXPECT_TRUE(component_updater().RegisterComponent(
      {.app_id = "jebgalgnebhfojomionfpkfelancnnkf",
       .public_key_hash = base::ToVector(update_client::jebg_hash),
       .version = base::Version("0.9"),
       .installer = base::MakeRefCounted<MockInstaller>(),
       .requires_network_encryption = false,
       .supports_group_policy_enable_component_updates = true}));
  component_updater().MaybeThrottle("jebgalgnebhfojomionfpkfelancnnkf",
                                    base::DoNothing());

  RunThreads();

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 0, 1);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 1);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 1);
}

TEST_F(ComponentUpdaterTest, ComponentDetails) {
  const std::string id = "abagagagagagagagagagagagagagagag";
  const std::string name = "test_name";

  const base::Version version("1.0");

  ASSERT_TRUE(component_updater().RegisterComponent(
      {.app_id = id,
       .name = name,
       .public_key_hash = base::ToVector(update_client::abag_hash),
       .version = version,
       .installer = base::MakeRefCounted<MockInstaller>(),
       .requires_network_encryption = false,
       .supports_group_policy_enable_component_updates = true}));

  CrxUpdateItem item;
  ASSERT_TRUE(component_updater().GetComponentDetails(id, &item));
  ASSERT_TRUE(item.component);
  const CrxComponent& registered = *item.component;

  EXPECT_EQ(registered.app_id, id);
  EXPECT_EQ(registered.version, version);
  EXPECT_EQ(registered.name, name);

#if BUILDFLAG(CHROME_FOR_TESTING)
  // In Chrome for Testing component updates are disabled.
  EXPECT_FALSE(registered.updates_enabled);
#else
  EXPECT_TRUE(registered.updates_enabled);
#endif  // BUILDFLAG(CHROME_FOR_TESTING)
}

TEST_F(ComponentUpdaterTest, UpdatesDisabled) {
  const std::string id = "abagagagagagagagagagagagagagagag";
  const std::string name = "test_name";

  const base::Version version("1.0");

  ASSERT_TRUE(component_updater().RegisterComponent(
      {.app_id = id,
       .name = name,
       .public_key_hash = base::ToVector(update_client::abag_hash),
       .version = version,
       .installer = base::MakeRefCounted<MockInstaller>(),
       .requires_network_encryption = false,
       .supports_group_policy_enable_component_updates = true,
       .allow_updates = false}));

  CrxUpdateItem item;
  ASSERT_TRUE(component_updater().GetComponentDetails(id, &item));
  ASSERT_TRUE(item.component);
  const CrxComponent& registered = *item.component;

  EXPECT_FALSE(registered.updates_enabled);
}

// Controlled by ComponentUpdatesEnabled policy.
// See more at https://chromeenterprise.google/policies/#ComponentUpdatesEnabled
TEST_F(ComponentUpdaterTest, UpdatesDisabledByPolicy) {
  const std::string id = "abagagagagagagagagagagagagagagag";
  const std::string name = "test_name";

  const base::Version version("1.0");
  // Simulate admin disabling component updates via policy.
  configurator()->GetPrefService()->SetBoolean(prefs::kComponentUpdatesEnabled,
                                               false);

  ASSERT_TRUE(component_updater().RegisterComponent(
      {.app_id = id,
       .name = name,
       .public_key_hash = base::ToVector(update_client::abag_hash),
       .version = version,
       .installer = base::MakeRefCounted<MockInstaller>(),
       .requires_network_encryption = false,
       // Enables admin control of component updates. In production, this value
       // is determined by the component implementation of
       // ComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates
       .supports_group_policy_enable_component_updates = true}));

  CrxUpdateItem item;
  ASSERT_TRUE(component_updater().GetComponentDetails(id, &item));
  ASSERT_TRUE(item.component);
  const CrxComponent& registered = *item.component;

  EXPECT_FALSE(registered.updates_enabled);
}

// Components that override SupportsGroupPolicyEnabledComponentUpdates to return
// false are always allowed to update, regardless of policy state.
// See more at https://chromeenterprise.google/policies/#ComponentUpdatesEnabled
TEST_F(ComponentUpdaterTest, CriticalComponentAlwaysUpdates) {
  const std::string id = "abagagagagagagagagagagagagagagag";
  const std::string name = "test_name";

  const base::Version version("1.0");
  // Simulate admin disabling component updates via policy.
  configurator()->GetPrefService()->SetBoolean(prefs::kComponentUpdatesEnabled,
                                               false);

  ASSERT_TRUE(component_updater().RegisterComponent(
      {.app_id = id,
       .name = name,
       .public_key_hash = base::ToVector(update_client::abag_hash),
       .version = version,
       .installer = base::MakeRefCounted<MockInstaller>(),
       .requires_network_encryption = false,
       // Exempt the component from the policy that disables updates.
       .supports_group_policy_enable_component_updates = false}));

  CrxUpdateItem item;
  ASSERT_TRUE(component_updater().GetComponentDetails(id, &item));
  ASSERT_TRUE(item.component);
  const CrxComponent& registered = *item.component;

  EXPECT_EQ(registered.app_id, id);
  EXPECT_EQ(registered.version, version);
  EXPECT_EQ(registered.name, name);

  EXPECT_TRUE(registered.updates_enabled);
}

TEST_F(ComponentUpdaterTest,
       RegisterCancelsPendingUnregistrationBeforeUpdateCompletes) {
  const std::string id = "abagagagagagagagagagagagagagagag";
  scoped_refptr<MockInstaller> installer =
      base::MakeRefCounted<MockInstaller>();
  EXPECT_CALL(*installer, Uninstall()).Times(0);

  const ComponentRegistration component = {
      .app_id = id,
      .public_key_hash = base::ToVector(update_client::abag_hash),
      .version = base::Version("1.0"),
      .installer = installer,
      .requires_network_encryption = false,
      .supports_group_policy_enable_component_updates = true};

  EXPECT_TRUE(component_updater().RegisterComponent(component));

  // Unregister during an in-progress update adds component ID to the pending
  // unregistration list.
  EXPECT_CALL(update_client(), IsUpdating(id)).WillOnce(Return(true));
  EXPECT_TRUE(component_updater().UnregisterComponent(id));

  // Re-register removes the component ID from the pending unregistration list
  // so the component is not uninstalled after the update completes.
  EXPECT_TRUE(component_updater().RegisterComponent(component));

  // The update does not uninstall the component.
  TriggerUpdateComplete(id);

  CrxUpdateItem item;
  EXPECT_TRUE(component_updater().GetComponentDetails(id, &item));
  EXPECT_TRUE(item.component);
}

TEST_F(ComponentUpdaterTest,
       RegisterCancelsPendingUnregistrationAfterUpdateCompletes) {
  const std::string id = "abagagagagagagagagagagagagagagag";
  scoped_refptr<MockInstaller> installer =
      base::MakeRefCounted<MockInstaller>();
  // The component is uninstalled after the first update completes when
  // processing the pending unregistration list.
  EXPECT_CALL(*installer, Uninstall()).WillOnce(Return(true));

  const ComponentRegistration component = {
      .app_id = id,
      .public_key_hash = base::ToVector(update_client::abag_hash),
      .version = base::Version("1.0"),
      .installer = installer,
      .requires_network_encryption = false,
      .supports_group_policy_enable_component_updates = true};

  EXPECT_TRUE(component_updater().RegisterComponent(component));

  // Unregister during an in progress update adds component ID to the pending
  // unregistration list.
  EXPECT_CALL(update_client(), IsUpdating(id)).WillOnce(Return(true));
  EXPECT_TRUE(component_updater().UnregisterComponent(id));

  EXPECT_CALL(update_client(), IsUpdating(id)).WillOnce(Return(false));
  // When the first update completes the component is uninstalled.
  TriggerUpdateComplete(id);

  CrxUpdateItem item;
  EXPECT_FALSE(component_updater().GetComponentDetails(id, &item));
  EXPECT_FALSE(item.component);

  // Re-register removes the component ID from the pending unregistration list
  // so the component is not uninstalled after the update completes.
  EXPECT_TRUE(component_updater().RegisterComponent(component));

  // The second update does not uninstall the component.
  TriggerUpdateComplete(id);

  EXPECT_TRUE(component_updater().GetComponentDetails(id, &item));
  EXPECT_TRUE(item.component);
}

TEST_F(ComponentUpdaterTest, Cancel) {
  const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
  EXPECT_CALL(update_client(), Cancel(id)).WillOnce(Return(true));
  EXPECT_CALL(update_client(), Stop());
  EXPECT_CALL(scheduler(), Stop());
  EXPECT_TRUE(component_updater().Cancel(id));
}

// Tests the behavior of the component updater when the cost of the connection
// changes. The tests run with mock time so that the delayed resumption of the
// updates deferred by a metered connection is deterministic.
class ComponentUpdaterMeteredTest : public ComponentUpdaterTest {
 public:
  ComponentUpdaterMeteredTest()
      : ComponentUpdaterTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  using ComponentState = ::update_client::ComponentState;
  using ConnectionCost = ::net::NetworkChangeNotifier::ConnectionCost;

  static constexpr char kComponent1[] = "jebgalgnebhfojomionfpkfelancnnkf";
  static constexpr char kComponent2[] = "abagagagagagagagagagagagagagagag";
  static constexpr char kComponent3[] = "ihfokbkgjpifnbbojhneepfflplebdkc";

  void SetUp() override {
    // Don't run the periodic update task.
    ON_CALL(scheduler(), Schedule(_, _, _, _)).WillByDefault(Return());
    EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(update_client(), Stop());
    EXPECT_CALL(scheduler(), Stop());
    // Component updates are enabled, so that `UPDATE_DISABLED` can only come
    // from the metered connection.
    configurator()->GetPrefService()->SetBoolean(
        prefs::kComponentUpdatesEnabled, true);
  }

  CrxUpdateService& crx_update_service() {
    return static_cast<CrxUpdateService&>(component_updater());
  }

  ComponentRegistration MakeRegistration(
      const std::string& id,
      bool allow_updates_on_metered_connection) {
    auto installer = base::MakeRefCounted<MockInstaller>();
    ON_CALL(*installer, Uninstall()).WillByDefault(Return(true));
    return ComponentRegistration(
        id, /*name=*/{}, base::ToVector(update_client::jebg_hash),
        base::Version("0.9"), /*fingerprint=*/{}, /*installer_attributes=*/{},
        /*action_handler=*/nullptr, std::move(installer),
        /*requires_network_encryption=*/false,
        /*supports_group_policy_enable_component_updates=*/true,
        /*allow_cached_copies=*/true, allow_updates_on_metered_connection,
        /*allow_updates=*/true);
  }

  // Makes the update client report that the update of `id` is in `state`.
  void SetUpdateState(const std::string& id, ComponentState state) {
    EXPECT_CALL(update_client(), GetCrxUpdateState(id, _))
        .WillRepeatedly([state](const std::string&, CrxUpdateItem* item) {
          item->state = state;
          return true;
        });
  }

  // Changes the cost of the connection as seen by the configurator, and
  // notifies the observers of the network change notifier. The notification is
  // delivered asynchronously, on the test sequence.
  void SetConnectionCost(ConnectionCost cost) {
    configurator()->SetIsNetworkConnectionMetered(
        cost == net::NetworkChangeNotifier::CONNECTION_COST_METERED);
    net::NetworkChangeNotifier::NotifyObserversOfConnectionCostChangeForTests(
        cost);
  }

  // Changes the connection type reported by `NetworkChangeNotifier` and
  // notifies `NetworkChangeObserver`s.
  void SetConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    network_change_notifier_.mock_network_change_notifier()->SetConnectionType(
        type);
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(type);
  }

  // Returns an update item reporting that the update client refused to update
  // `id` with `ServiceError::UPDATE_DISABLED`.
  CrxUpdateItem MakeUpdateDisabledItem(
      const std::string& id,
      bool allow_updates_on_metered_connection = false) {
    CrxUpdateItem item;
    item.id = id;
    item.state = ComponentState::kUpdateError;
    item.error_category = update_client::ErrorCategory::kService;
    item.error_code =
        std::to_underlying(update_client::ServiceError::UPDATE_DISABLED);
    item.component.emplace();
    item.component->app_id = id;
    item.component->allow_updates_on_metered_connection =
        allow_updates_on_metered_connection;
    item.component->updates_enabled =
        configurator()->GetPrefService()->GetBoolean(
            prefs::kComponentUpdatesEnabled);
    return item;
  }

 private:
  // Needed so that `NotifyObserversOfConnectionCostChangeForTests()` has a
  // global `NetworkChangeNotifier` instance.
  net::test::ScopedMockNetworkChangeNotifier network_change_notifier_;
};

// A connection that becomes metered stops the downloads of the components
// that are not allowed to update on metered connections, and only those.
TEST_F(ComponentUpdaterMeteredTest, MeteredConnectionStopsDownloads) {
  ASSERT_TRUE(component_updater().RegisterComponent(
      MakeRegistration(kComponent1,
                       /*allow_updates_on_metered_connection=*/false)));
  ASSERT_TRUE(component_updater().RegisterComponent(
      MakeRegistration(kComponent2,
                       /*allow_updates_on_metered_connection=*/true)));
  ASSERT_TRUE(component_updater().RegisterComponent(
      MakeRegistration(kComponent3,
                       /*allow_updates_on_metered_connection=*/false)));

  SetUpdateState(kComponent1, ComponentState::kDownloading);
  SetUpdateState(kComponent2, ComponentState::kDownloading);
  SetUpdateState(kComponent3, ComponentState::kUpdating);

  base::RunLoop run_loop;
  EXPECT_CALL(update_client(), Cancel(kComponent1))
      .WillOnce([&run_loop](const std::string&) {
        run_loop.Quit();
        return true;
      });
  // Neither the component allowed on metered connections nor the component
  // that is past its download are cancelled. All components are visited in
  // the single notification that cancels `kComponent1`, so quitting the loop
  // on that cancellation is enough to verify the two negative expectations.
  EXPECT_CALL(update_client(), Cancel(kComponent2)).Times(0);
  EXPECT_CALL(update_client(), Cancel(kComponent3)).Times(0);

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  run_loop.Run();
}

// A download stopped by a metered connection is requested again, at the
// priority it was requested with, once the connection is no longer metered.
TEST_F(ComponentUpdaterMeteredTest, UnmeteredConnectionResumesStoppedDownload) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  SetUpdateState(kComponent1, ComponentState::kDownloading);

  base::RunLoop cancel_loop;
  base::RunLoop resume_loop;
  {
    InSequence seq;
    // The foreground on-demand update stays in flight until it is cancelled.
    EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                        /*is_foreground=*/true, _))
        .WillOnce(Return());
    EXPECT_CALL(update_client(), Cancel(kComponent1))
        .WillOnce([&cancel_loop](const std::string&) {
          cancel_loop.Quit();
          return true;
        });
    // The update is requested again with the same priority.
    EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                        /*is_foreground=*/true, _))
        .WillOnce(
            [&resume_loop](const std::vector<std::string>&,
                           UpdateClient::CrxDataCallback,
                           UpdateClient::CrxStateChangeCallback, bool,
                           update_client::Callback) { resume_loop.Quit(); });
  }

  OnDemandTester tester;
  tester.OnDemand(&component_updater(), kComponent1,
                  OnDemandUpdater::Priority::FOREGROUND);

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  cancel_loop.Run();

  // The resumption is delayed; mock time advances to it when the loop is idle.
  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  resume_loop.Run();
}

// When two foreground on-demand updates are requested for the same component,
// the completion of the first does not drop the foreground priority of the
// second.
TEST_F(ComponentUpdaterMeteredTest,
       QueuedForegroundUpdatePreservesPriorityWhenFirstCompletes) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  SetUpdateState(kComponent1, ComponentState::kDownloading);

  update_client::Callback first_callback;
  update_client::Callback second_callback;
  base::RunLoop cancel_loop;
  base::RunLoop resume_loop;
  {
    InSequence seq;
    EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                        /*is_foreground=*/true, _))
        .WillOnce([&first_callback](const std::vector<std::string>&,
                                    UpdateClient::CrxDataCallback,
                                    UpdateClient::CrxStateChangeCallback, bool,
                                    update_client::Callback callback) {
          first_callback = std::move(callback);
        });
    EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                        /*is_foreground=*/true, _))
        .WillOnce([&second_callback](const std::vector<std::string>&,
                                     UpdateClient::CrxDataCallback,
                                     UpdateClient::CrxStateChangeCallback, bool,
                                     update_client::Callback callback) {
          second_callback = std::move(callback);
        });
    EXPECT_CALL(update_client(), Cancel(kComponent1))
        .WillOnce([&second_callback, &cancel_loop](const std::string&) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(second_callback),
                                        update_client::Error::NONE));
          cancel_loop.Quit();
          return true;
        });
    EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                        /*is_foreground=*/true, _))
        .WillOnce(
            [&resume_loop](const std::vector<std::string>&,
                           UpdateClient::CrxDataCallback,
                           UpdateClient::CrxStateChangeCallback, bool,
                           update_client::Callback) { resume_loop.Quit(); });
  }

  OnDemandTester first_tester;
  OnDemandTester second_tester;
  first_tester.OnDemand(&component_updater(), kComponent1,
                        OnDemandUpdater::Priority::FOREGROUND);
  second_tester.OnDemand(&component_updater(), kComponent1,
                         OnDemandUpdater::Priority::FOREGROUND);

  // The first update completes before the connection turns metered.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(first_callback), update_client::Error::NONE));
  first_tester.Wait();
  EXPECT_EQ(update_client::Error::NONE, first_tester.error());

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  cancel_loop.Run();
  second_tester.Wait();
  EXPECT_EQ(update_client::Error::NONE, second_tester.error());

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  resume_loop.Run();
}

// An update refused by the update client because the connection is metered is
// requested again, at background priority, once the connection is no longer
// metered. A component that allows metered updates is never deferred.
TEST_F(ComponentUpdaterMeteredTest,
       UnmeteredConnectionResumesUpdateRefusedOnMeteredConnection) {
  ASSERT_TRUE(component_updater().RegisterComponent(
      MakeRegistration(kComponent1,
                       /*allow_updates_on_metered_connection=*/false)));
  ASSERT_TRUE(component_updater().RegisterComponent(
      MakeRegistration(kComponent2,
                       /*allow_updates_on_metered_connection=*/true)));

  base::RunLoop resume_loop;
  EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                      /*is_foreground=*/false, _))
      .WillOnce(
          [&resume_loop](const std::vector<std::string>&,
                         UpdateClient::CrxDataCallback,
                         UpdateClient::CrxStateChangeCallback, bool,
                         update_client::Callback) { resume_loop.Quit(); });
  EXPECT_CALL(update_client(), Update(ElementsAre(kComponent2), _, _, _, _))
      .Times(0);

  configurator()->SetIsNetworkConnectionMetered(true);
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));
  crx_update_service().OnEvent(MakeUpdateDisabledItem(
      kComponent2, /*allow_updates_on_metered_connection=*/true));

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  resume_loop.Run();
}

// An update refused while component updates are disabled by policy is not
// deferred.
TEST_F(ComponentUpdaterMeteredTest, PolicyDisabledUpdateIsNotDeferred) {
  configurator()->GetPrefService()->SetBoolean(prefs::kComponentUpdatesEnabled,
                                               false);
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  EXPECT_CALL(update_client(), Update).Times(0);

  configurator()->SetIsNetworkConnectionMetered(true);
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  task_environment().FastForwardBy(base::Minutes(1));
}

// Unregistering a component drops any pending deferral for it, including when
// unregistration is deferred because an update is still in progress.
TEST_F(ComponentUpdaterMeteredTest,
       UnregisteringComponentClearsPendingDeferral) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent2, /*allow_updates_on_metered_connection=*/false)));
  EXPECT_CALL(update_client(), IsUpdating(kComponent1))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(update_client(), IsUpdating(kComponent2))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(update_client(), Update).Times(0);

  configurator()->SetIsNetworkConnectionMetered(true);
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent2));
  EXPECT_TRUE(component_updater().UnregisterComponent(kComponent1));
  EXPECT_TRUE(component_updater().UnregisterComponent(kComponent2));

  // A late refusal event for a component pending unregistration must not
  // re-defer it either.
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent2));

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  task_environment().FastForwardBy(base::Minutes(1));
}

// Resuming several deferred components batches the background updates into one
// call while sending foreground updates individually.
TEST_F(ComponentUpdaterMeteredTest,
       UnmeteredConnectionBatchesBackgroundAndSeparatesForegroundUpdates) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent2, /*allow_updates_on_metered_connection=*/false)));
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent3, /*allow_updates_on_metered_connection=*/false)));

  EXPECT_CALL(update_client(), GetCrxUpdateState(kComponent1, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(update_client(), GetCrxUpdateState(kComponent2, _))
      .WillRepeatedly(Return(false));
  SetUpdateState(kComponent3, ComponentState::kDownloading);

  base::RunLoop cancel_loop;
  EXPECT_CALL(update_client(),
              Update(ElementsAre(kComponent3), _, _, /*is_foreground=*/true, _))
      .WillOnce(Return());
  EXPECT_CALL(update_client(), Cancel(kComponent3))
      .WillOnce([&cancel_loop](const std::string&) {
        cancel_loop.Quit();
        return true;
      });

  OnDemandTester tester;
  tester.OnDemand(&component_updater(), kComponent3,
                  OnDemandUpdater::Priority::FOREGROUND);

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  cancel_loop.Run();

  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent2));

  EXPECT_CALL(update_client(),
              Update(ElementsAre(kComponent3), _, _, /*is_foreground=*/true, _))
      .Times(1);
  EXPECT_CALL(update_client(),
              Update(UnorderedElementsAre(kComponent1, kComponent2), _, _,
                     /*is_foreground=*/false, _))
      .Times(1);

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  task_environment().FastForwardBy(base::Seconds(5));
}

// An update refusal event that arrives after the connection has already turned
// unmetered still starts the resumption timer.
TEST_F(ComponentUpdaterMeteredTest,
       LateRefusalAfterUnmeteredNotificationStillResumes) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));

  // The connection is already unmetered when the refusal event from the earlier
  // metered update check is delivered.
  configurator()->SetIsNetworkConnectionMetered(false);

  base::RunLoop resume_loop;
  EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                      /*is_foreground=*/false, _))
      .WillOnce(
          [&resume_loop](const std::vector<std::string>&,
                         UpdateClient::CrxDataCallback,
                         UpdateClient::CrxStateChangeCallback, bool,
                         update_client::Callback) { resume_loop.Quit(); });

  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));
  resume_loop.Run();
}

// A component that finishes updating before the resumption timer fires is
// removed from the deferred set.
TEST_F(ComponentUpdaterMeteredTest, CompletedUpdateClearsPendingDeferral) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  EXPECT_CALL(update_client(), Update).Times(0);

  configurator()->SetIsNetworkConnectionMetered(true);
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));

  CrxUpdateItem updated_item;
  updated_item.id = kComponent1;
  updated_item.state = ComponentState::kUpdated;
  updated_item.next_version = base::Version("1.0");
  crx_update_service().OnEvent(updated_item);

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  task_environment().FastForwardBy(base::Minutes(1));
}

// A connection that becomes metered again before the deferred updates resume
// stops the resumption timer and keeps them deferred until the next unmetered
// transition.
TEST_F(ComponentUpdaterMeteredTest,
       MeteredConnectionAgainKeepsUpdatesDeferred) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  // The update client is not updating the component when the connection
  // becomes metered again.
  base::RunLoop metered_loop;
  EXPECT_CALL(update_client(), GetCrxUpdateState(kComponent1, _))
      .WillOnce([&metered_loop](const std::string&, CrxUpdateItem*) {
        metered_loop.Quit();
        return false;
      });

  configurator()->SetIsNetworkConnectionMetered(true);
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));

  // Start the 5s resumption timer, then switch back to metered after 2s.
  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  task_environment().FastForwardBy(base::Seconds(2));
  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  metered_loop.Run();

  // Switch back to unmetered at t = 4s. Because the first timer was stopped at
  // t = 2s, nothing resumes at t = 5s (1s later); the update resumes at t = 9s
  // (5s after the second unmetered notification).
  task_environment().FastForwardBy(base::Seconds(2));
  bool resumed = false;
  EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                      /*is_foreground=*/false, _))
      .WillOnce([&resumed](const std::vector<std::string>&,
                           UpdateClient::CrxDataCallback,
                           UpdateClient::CrxStateChangeCallback, bool,
                           update_client::Callback) { resumed = true; });

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  task_environment().FastForwardBy(base::Seconds(3));
  EXPECT_FALSE(resumed);

  task_environment().FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(resumed);
}

// A background deferral does not downgrade an already-deferred foreground
// update.
TEST_F(ComponentUpdaterMeteredTest, DeferredForegroundUpdateStaysForeground) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  SetUpdateState(kComponent1, ComponentState::kDownloading);

  update_client::Callback update_callback;
  base::RunLoop resume_loop;
  {
    InSequence seq;
    EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                        /*is_foreground=*/true, _))
        .WillOnce([&update_callback](const std::vector<std::string>&,
                                     UpdateClient::CrxDataCallback,
                                     UpdateClient::CrxStateChangeCallback, bool,
                                     update_client::Callback callback) {
          update_callback = std::move(callback);
        });
    EXPECT_CALL(update_client(), Cancel(kComponent1))
        .WillOnce([&update_callback](const std::string&) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(update_callback),
                                        update_client::Error::NONE));
          return true;
        });
    EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                        /*is_foreground=*/true, _))
        .WillOnce(
            [&resume_loop](const std::vector<std::string>&,
                           UpdateClient::CrxDataCallback,
                           UpdateClient::CrxStateChangeCallback, bool,
                           update_client::Callback) { resume_loop.Quit(); });
  }

  OnDemandTester tester;
  tester.OnDemand(&component_updater(), kComponent1,
                  OnDemandUpdater::Priority::FOREGROUND);

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  tester.Wait();

  // A subsequent background `UPDATE_DISABLED` event for the same component
  // must not downgrade its deferred priority from FOREGROUND to BACKGROUND.
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  resume_loop.Run();
}

// Consecutive non-metered notifications (e.g. UNKNOWN followed by UNMETERED)
// do not restart the 5-second resumption timer.
TEST_F(ComponentUpdaterMeteredTest,
       SecondNonMeteredNotificationDoesNotRestartTimer) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));

  bool resumed = false;
  EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                      /*is_foreground=*/false, _))
      .WillOnce([&resumed](const std::vector<std::string>&,
                           UpdateClient::CrxDataCallback,
                           UpdateClient::CrxStateChangeCallback, bool,
                           update_client::Callback) { resumed = true; });

  configurator()->SetIsNetworkConnectionMetered(true);
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));

  // Transitioning to UNKNOWN starts the 5s timer; a follow-up UNMETERED
  // notification 3s later must not push resumption past t = 5s.
  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNKNOWN);
  task_environment().FastForwardBy(base::Seconds(3));
  EXPECT_FALSE(resumed);

  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  task_environment().FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(resumed);
}

// Disconnecting from a metered hotspot into an offline state (which reports
// `CONNECTION_COST_UNMETERED`) does not run `ResumeDeferredUpdates()` while
// offline, and connecting to an unmetered Wi-Fi network later resumes the
// deferred updates via `OnNetworkChanged()`.
TEST_F(ComponentUpdaterMeteredTest,
       HotspotToOfflineToWifiResumesDeferredUpdates) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));

  configurator()->SetIsNetworkConnectionMetered(true);
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));

  // Going offline reports `CONNECTION_COST_UNMETERED`, but must not resume
  // deferred updates while `IsOffline()` is true.
  bool resumed = false;
  EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                      /*is_foreground=*/false, _))
      .WillOnce([&resumed](const std::vector<std::string>&,
                           UpdateClient::CrxDataCallback,
                           UpdateClient::CrxStateChangeCallback, bool,
                           update_client::Callback) { resumed = true; });

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  task_environment().FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(resumed);

  // Connecting to an unmetered Wi-Fi network fires `OnNetworkChanged` (without
  // necessarily firing `OnConnectionCostChanged` again) and resumes the
  // deferred update after the 5-second delay.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(resumed);
}

// An explicit `OnDemandUpdate()` while online and unmetered clears a pending
// deferral of equal or lower priority so the 5-second timer does not queue a
// duplicate update, while preserving a foreground deferral if only a background
// on-demand update is requested.
TEST_F(ComponentUpdaterMeteredTest,
       OnDemandUpdateWhileUnmeteredClearsPendingDeferral) {
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent1, /*allow_updates_on_metered_connection=*/false)));
  ASSERT_TRUE(component_updater().RegisterComponent(MakeRegistration(
      kComponent2, /*allow_updates_on_metered_connection=*/false)));
  SetUpdateState(kComponent2, ComponentState::kDownloading);

  base::RunLoop cancel_loop;
  update_client::Callback k2_callback;
  EXPECT_CALL(update_client(),
              Update(ElementsAre(kComponent2), _, _, /*is_foreground=*/true, _))
      .WillOnce([&k2_callback](const std::vector<std::string>&,
                               UpdateClient::CrxDataCallback,
                               UpdateClient::CrxStateChangeCallback, bool,
                               update_client::Callback callback) {
        k2_callback = std::move(callback);
      });
  EXPECT_CALL(update_client(), Cancel(kComponent2))
      .WillOnce([&k2_callback, &cancel_loop](const std::string&) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(k2_callback), update_client::Error::NONE));
        cancel_loop.Quit();
        return true;
      });
  EXPECT_CALL(update_client(), GetCrxUpdateState(kComponent1, _))
      .WillRepeatedly(Return(false));

  OnDemandTester fg_tester;
  fg_tester.OnDemand(&component_updater(), kComponent2,
                     OnDemandUpdater::Priority::FOREGROUND);

  // Defer `kComponent1` as BACKGROUND and `kComponent2` as FOREGROUND.
  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  cancel_loop.Run();
  crx_update_service().OnEvent(MakeUpdateDisabledItem(kComponent1));

  // Switch to unmetered, starting the 5-second resumption timer.
  SetConnectionCost(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);

  // Before the 5-second timer fires:
  // 1. A FOREGROUND `OnDemandUpdate` for `kComponent1` clears its BACKGROUND
  //    deferral, so `kComponent1` is updated only once (now) and not a second
  //    time at t = 5s.
  // 2. A BACKGROUND `OnDemandUpdate` for `kComponent2` does NOT clear its
  //    FOREGROUND deferral, so `kComponent2` still resumes as FOREGROUND at
  //    t = 5s.
  EXPECT_CALL(update_client(),
              Update(ElementsAre(kComponent1), _, _, /*is_foreground=*/true, _))
      .Times(1);
  EXPECT_CALL(update_client(), Update(ElementsAre(kComponent1), _, _,
                                      /*is_foreground=*/false, _))
      .Times(0);
  EXPECT_CALL(update_client(), Update(ElementsAre(kComponent2), _, _,
                                      /*is_foreground=*/false, _))
      .Times(1);
  EXPECT_CALL(update_client(),
              Update(ElementsAre(kComponent2), _, _, /*is_foreground=*/true, _))
      .Times(1);

  OnDemandTester k1_fg_tester;
  k1_fg_tester.OnDemand(&component_updater(), kComponent1,
                        OnDemandUpdater::Priority::FOREGROUND);
  OnDemandTester k2_bg_tester;
  k2_bg_tester.OnDemand(&component_updater(), kComponent2,
                        OnDemandUpdater::Priority::BACKGROUND);

  task_environment().FastForwardBy(base::Seconds(5));
}

}  // namespace component_updater
