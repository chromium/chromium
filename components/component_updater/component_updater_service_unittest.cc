// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_service.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Configurator = update_client::Configurator;
using Result = update_client::CrxInstaller::Result;
using TestConfigurator = update_client::TestConfigurator;
using UpdateClient = update_client::UpdateClient;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Unused;

namespace component_updater {

class MockInstaller : public CrxInstaller {
 public:
  MockInstaller();

  // gMock does not support mocking functions with parameters which have
  // move semantics. This function is a shim to work around it.
  void Install(const base::FilePath& unpack_path,
               const std::string& public_key,
               update_client::CrxInstaller::Callback callback) override {
    DoInstall(unpack_path, callback);
  }

  MOCK_METHOD1(OnUpdateError, void(int error));
  MOCK_METHOD2(DoInstall,
               void(const base::FilePath& unpack_path,
                    const update_client::CrxInstaller::Callback& callback));
  MOCK_METHOD2(GetInstalledFile,
               bool(const std::string& file, base::FilePath* installed_file));
  MOCK_METHOD0(Uninstall, bool());

 private:
  ~MockInstaller() override;
};

class MockUpdateClient : public UpdateClient {
 public:
  MockUpdateClient();

  // gMock does not support mocking functions with parameters which have
  // move semantics. This function is a shim to work around it.
  void Install(const std::string& id,
               CrxDataCallback crx_data_callback,
               Callback callback) override {
    DoInstall(id);
    std::move(callback).Run(update_client::Error::NONE);
  }

  void Update(const std::vector<std::string>& ids,
              CrxDataCallback crx_data_callback,
              bool is_foreground,
              Callback callback) override {
    // All update calls initiated by the component update service are
    // automatically triggered as background updates without user intervention.
    EXPECT_FALSE(is_foreground);
    DoUpdate(ids);
    std::move(callback).Run(update_client::Error::NONE);
  }

  void SendUninstallPing(const std::string& id,
                         const base::Version& version,
                         int reason,
                         Callback callback) override {
    DoSendUninstallPing(id, version, reason);
    std::move(callback).Run(update_client::Error::NONE);
  }

  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD1(DoInstall, void(const std::string& id));
  MOCK_METHOD1(DoUpdate, void(const std::vector<std::string>& ids));
  MOCK_CONST_METHOD2(GetCrxUpdateState,
                     bool(const std::string& id, CrxUpdateItem* update_item));
  MOCK_CONST_METHOD1(IsUpdating, bool(const std::string& id));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD3(DoSendUninstallPing,
               void(const std::string& id,
                    const base::Version& version,
                    int reason));

 private:
  ~MockUpdateClient() override;
};

class MockServiceObserver : public ServiceObserver {
 public:
  MockServiceObserver();
  ~MockServiceObserver() override;

  MOCK_METHOD2(OnEvent, void(Events event, const std::string&));
};

class MockUpdateScheduler : public UpdateScheduler {
 public:
  MOCK_METHOD4(Schedule,
               void(const base::TimeDelta& initial_delay,
                    const base::TimeDelta& delay,
                    const UserTask& user_task,
                    const OnStopTaskCallback& on_stop));
  MOCK_METHOD0(Stop, void());
};

class ComponentUpdaterTest : public testing::Test {
 public:
  ComponentUpdaterTest();
  ~ComponentUpdaterTest() override;

  void SetUp() override;

  void TearDown() override;

  // Makes the full path to a component updater test file.
  const base::FilePath test_file(const char* file);

  MockUpdateClient& update_client() { return *update_client_; }
  ComponentUpdateService& component_updater() { return *component_updater_; }
  scoped_refptr<TestConfigurator> configurator() const { return config_; }
  base::OnceClosure quit_closure() { return runloop_.QuitClosure(); }
  MockUpdateScheduler& scheduler() { return *scheduler_; }
  static void ReadyCallback() {}

 protected:
  void RunThreads();

 private:
  void RunUpdateTask(const UpdateScheduler::UserTask& user_task);
  void Schedule(const base::TimeDelta& initial_delay,
                const base::TimeDelta& delay,
                const UpdateScheduler::UserTask& user_task,
                const UpdateScheduler::OnStopTaskCallback& on_stop);

  base::test::TaskEnvironment task_environment_;
  base::RunLoop runloop_;

  scoped_refptr<TestConfigurator> config_ =
      base::MakeRefCounted<TestConfigurator>();
  MockUpdateScheduler* scheduler_;
  scoped_refptr<MockUpdateClient> update_client_ =
      base::MakeRefCounted<MockUpdateClient>();
  std::unique_ptr<ComponentUpdateService> component_updater_;

  DISALLOW_COPY_AND_ASSIGN(ComponentUpdaterTest);
};

class OnDemandTester {
 public:
  void OnDemand(ComponentUpdateService* cus,
                const std::string& id,
                OnDemandUpdater::Priority priority);
  update_client::Error error() const { return error_; }

 private:
  void OnDemandComplete(update_client::Error error);

  update_client::Error error_ = update_client::Error::NONE;
};

MockInstaller::MockInstaller() {
}

MockInstaller::~MockInstaller() {
}

MockUpdateClient::MockUpdateClient() {
}

MockUpdateClient::~MockUpdateClient() {
}

MockServiceObserver::MockServiceObserver() {
}

MockServiceObserver::~MockServiceObserver() {
}

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
}

std::unique_ptr<ComponentUpdateService> TestComponentUpdateServiceFactory(
    scoped_refptr<Configurator> config) {
  DCHECK(config);
  return std::make_unique<CrxUpdateService>(
      config, std::make_unique<MockUpdateScheduler>(),
      base::MakeRefCounted<MockUpdateClient>());
}

ComponentUpdaterTest::ComponentUpdaterTest() {
  EXPECT_CALL(update_client(), AddObserver(_)).Times(1);
  auto scheduler = std::make_unique<MockUpdateScheduler>();
  scheduler_ = scheduler.get();
  ON_CALL(*scheduler_, Schedule(_, _, _, _))
      .WillByDefault(Invoke(this, &ComponentUpdaterTest::Schedule));
  component_updater_ = std::make_unique<CrxUpdateService>(
      config_, std::move(scheduler), update_client_);
}

ComponentUpdaterTest::~ComponentUpdaterTest() {
  EXPECT_CALL(update_client(), RemoveObserver(_)).Times(1);
  component_updater_.reset();
}

void ComponentUpdaterTest::SetUp() {
}

void ComponentUpdaterTest::TearDown() {
}

void ComponentUpdaterTest::RunThreads() {
  runloop_.Run();
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
    const base::TimeDelta& initial_delay,
    const base::TimeDelta& delay,
    const UpdateScheduler::UserTask& user_task,
    const UpdateScheduler::OnStopTaskCallback& on_stop) {
  RunUpdateTask(user_task);
}

TEST_F(ComponentUpdaterTest, AddObserver) {
  MockServiceObserver observer;
  EXPECT_CALL(update_client(), AddObserver(&observer)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);
  component_updater().AddObserver(&observer);
}

TEST_F(ComponentUpdaterTest, RemoveObserver) {
  MockServiceObserver observer;
  EXPECT_CALL(update_client(), RemoveObserver(&observer)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);
  component_updater().RemoveObserver(&observer);
}

// Tests that UpdateClient::Update is called by the timer loop when
// components are registered, and the component update starts.
// Also tests that Uninstall is called when a component is unregistered.
TEST_F(ComponentUpdaterTest, RegisterComponent) {
  class LoopHandler {
   public:
    LoopHandler(int max_cnt, base::OnceClosure quit_closure)
        : max_cnt_(max_cnt), quit_closure_(std::move(quit_closure)) {}

    void OnUpdate(const std::vector<std::string>& ids) {
      static int cnt = 0;
      ++cnt;
      if (cnt >= max_cnt_)
        std::move(quit_closure_).Run();
    }

   private:
    const int max_cnt_;
    base::OnceClosure quit_closure_;
  };

  base::HistogramTester ht;

  scoped_refptr<MockInstaller> installer =
      base::MakeRefCounted<MockInstaller>();
  EXPECT_CALL(*installer, Uninstall()).WillOnce(Return(true));

  using update_client::jebg_hash;
  using update_client::abag_hash;

  const std::string id1 = "abagagagagagagagagagagagagagagag";
  const std::string id2 = "jebgalgnebhfojomionfpkfelancnnkf";
  std::vector<std::string> ids;
  ids.push_back(id1);
  ids.push_back(id2);

  CrxComponent crx_component1;
  crx_component1.app_id = id1;
  crx_component1.pk_hash.assign(abag_hash, abag_hash + base::size(abag_hash));
  crx_component1.version = base::Version("1.0");
  crx_component1.installer = installer;

  CrxComponent crx_component2;
  crx_component2.app_id = id2;
  crx_component2.pk_hash.assign(jebg_hash, jebg_hash + base::size(jebg_hash));
  crx_component2.version = base::Version("0.9");
  crx_component2.installer = installer;

  // Quit after two update checks have fired.
  LoopHandler loop_handler(2, quit_closure());
  EXPECT_CALL(update_client(), DoUpdate(ids))
      .WillRepeatedly(Invoke(&loop_handler, &LoopHandler::OnUpdate));

  EXPECT_CALL(update_client(), IsUpdating(id1)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);

  EXPECT_TRUE(component_updater().RegisterComponent(crx_component1));
  EXPECT_TRUE(component_updater().RegisterComponent(crx_component2));

  RunThreads();
  EXPECT_TRUE(component_updater().UnregisterComponent(id1));

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 1, 2);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 2);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 2);
}

// Tests that on-demand updates invoke UpdateClient::Install.
TEST_F(ComponentUpdaterTest, OnDemandUpdate) {
  class LoopHandler {
   public:
    explicit LoopHandler(int max_cnt) : max_cnt_(max_cnt) {}

    void OnInstall(const std::string& ids) {
      ++cnt_;
      if (cnt_ >= max_cnt_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&LoopHandler::Quit, base::Unretained(this)));
      }
    }

    void OnUpdate(const std::vector<std::string>& ids) {
      ++cnt_;
      if (cnt_ >= max_cnt_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&LoopHandler::Quit, base::Unretained(this)));
      }
    }

   private:
    void Quit() { base::RunLoop::QuitCurrentWhenIdleDeprecated(); }

    int cnt_ = 0;
    const int max_cnt_;
  };

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

  // Register two components, then call |OnDemand| for each component, with
  // foreground and background priorities. Expect calls to |Schedule| because
  // components have registered, calls to |Install| and |Update| corresponding
  // to each |OnDemand| invocation, and calls to |Stop| when the mocks are
  // torn down.
  LoopHandler loop_handler(2);
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(1);
  EXPECT_CALL(update_client(), DoInstall("jebgalgnebhfojomionfpkfelancnnkf"))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnInstall));
  EXPECT_CALL(
      update_client(),
      DoUpdate(std::vector<std::string>({"abagagagagagagagagagagagagagagag"})))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnUpdate));
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);

  {
    using update_client::jebg_hash;
    CrxComponent crx_component;
    crx_component.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
    crx_component.pk_hash.assign(jebg_hash, jebg_hash + base::size(jebg_hash));
    crx_component.version = base::Version("0.9");
    crx_component.installer = base::MakeRefCounted<MockInstaller>();
    EXPECT_TRUE(cus.RegisterComponent(crx_component));
  }
  {
    using update_client::abag_hash;
    CrxComponent crx_component;
    crx_component.app_id = "abagagagagagagagagagagagagagagag";
    crx_component.pk_hash.assign(abag_hash, abag_hash + base::size(abag_hash));
    crx_component.version = base::Version("0.9");
    crx_component.installer = base::MakeRefCounted<MockInstaller>();
    EXPECT_TRUE(cus.RegisterComponent(crx_component));
  }

  OnDemandTester ondemand_tester;
  ondemand_tester.OnDemand(&cus, "jebgalgnebhfojomionfpkfelancnnkf",
                           OnDemandUpdater::Priority::FOREGROUND);
  ondemand_tester.OnDemand(&cus, "abagagagagagagagagagagagagagagag",
                           OnDemandUpdater::Priority::BACKGROUND);
  base::RunLoop().Run();

  EXPECT_EQ(update_client::Error::INVALID_ARGUMENT,
            ondemand_tester_component_not_registered.error());
  EXPECT_EQ(update_client::Error::NONE, ondemand_tester.error());

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 0, 2);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 2);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 2);
}

// Tests that throttling an update invokes UpdateClient::Install.
TEST_F(ComponentUpdaterTest, MaybeThrottle) {
  class LoopHandler {
   public:
    LoopHandler(int max_cnt, base::OnceClosure quit_closure)
        : max_cnt_(max_cnt), quit_closure_(std::move(quit_closure)) {}

    void OnInstall(const std::string& ids) {
      static int cnt = 0;
      ++cnt;
      if (cnt >= max_cnt_)
        std::move(quit_closure_).Run();
    }

   private:
    const int max_cnt_;
    base::OnceClosure quit_closure_;
  };

  base::HistogramTester ht;

  // Don't run periodic update task.
  ON_CALL(scheduler(), Schedule(_, _, _, _)).WillByDefault(Return());

  scoped_refptr<MockInstaller> installer =
      base::MakeRefCounted<MockInstaller>();

  using update_client::jebg_hash;
  CrxComponent crx_component;
  crx_component.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + base::size(jebg_hash));
  crx_component.version = base::Version("0.9");
  crx_component.installer = installer;

  LoopHandler loop_handler(1, quit_closure());
  EXPECT_CALL(update_client(), DoInstall("jebgalgnebhfojomionfpkfelancnnkf"))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnInstall));
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);

  EXPECT_TRUE(component_updater().RegisterComponent(crx_component));
  component_updater().MaybeThrottle(
      "jebgalgnebhfojomionfpkfelancnnkf",
      base::BindOnce(&ComponentUpdaterTest::ReadyCallback));

  RunThreads();

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 0, 1);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 1);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 1);
}

}  // namespace component_updater
