// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power/power_manager_client.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

namespace {

// Shorthand for a few commonly-used constants.
const char* kInterface = power_manager::kPowerManagerInterface;
const char* kSuspendImminent = power_manager::kSuspendImminentSignal;
const char* kDarkSuspendImminent = power_manager::kDarkSuspendImminentSignal;
const char* kHandleSuspendReadiness =
    power_manager::kHandleSuspendReadinessMethod;
const char* kHandleDarkSuspendReadiness =
    power_manager::kHandleDarkSuspendReadinessMethod;

// Matcher that verifies that a dbus::Message has member |name|.
MATCHER_P(HasMember, name, "") {
  if (arg->GetMember() != name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  return true;
}

// Matcher that verifies that a dbus::MethodCall has member |method_name| and
// contains a SuspendReadinessInfo protobuf referring to |suspend_id| and
// |delay_id|.
MATCHER_P3(IsSuspendReadiness, method_name, suspend_id, delay_id, "") {
  if (arg->GetMember() != method_name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  power_manager::SuspendReadinessInfo proto;
  if (!dbus::MessageReader(arg).PopArrayOfBytesAsProto(&proto)) {
    *result_listener << "does not contain SuspendReadinessInfo protobuf";
    return false;
  }
  if (proto.suspend_id() != suspend_id) {
    *result_listener << "suspend ID is " << proto.suspend_id();
    return false;
  }
  if (proto.delay_id() != delay_id) {
    *result_listener << "delay ID is " << proto.delay_id();
    return false;
  }
  return true;
}

// Runs |callback| with |response|. Needed due to ResponseCallback expecting a
// bare pointer rather than an std::unique_ptr.
void RunResponseCallback(dbus::ObjectProxy::ResponseCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  std::move(callback).Run(response.get());
}

// Stub implementation of PowerManagerClient::Observer.
class TestObserver : public PowerManagerClient::Observer {
 public:
  explicit TestObserver(PowerManagerClient* client) : client_(client) {
    client_->AddObserver(this);
  }
  ~TestObserver() override { client_->RemoveObserver(this); }

  int num_suspend_imminent() const { return num_suspend_imminent_; }
  int num_suspend_done() const { return num_suspend_done_; }
  int num_dark_suspend_imminent() const { return num_dark_suspend_imminent_; }
  const base::UnguessableToken& block_suspend_token() const {
    return block_suspend_token_;
  }
  int32_t ambient_color_temperature() const {
    return ambient_color_temperature_;
  }

  void set_should_block_suspend(bool take_callback) {
    should_block_suspend_ = take_callback;
  }
  void set_run_unblock_suspend_immediately(bool run) {
    run_unblock_suspend_immediately_ = run;
  }

  // Runs |block_suspend_token_|.
  bool UnblockSuspend() WARN_UNUSED_RESULT {
    if (block_suspend_token_.is_empty())
      return false;

    client_->UnblockSuspend(block_suspend_token_);
    return true;
  }

  // PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override {
    num_suspend_imminent_++;
    if (should_block_suspend_) {
      block_suspend_token_ = base::UnguessableToken::Create();
      client_->BlockSuspend(block_suspend_token_, FROM_HERE.ToString());
    }
    if (run_unblock_suspend_immediately_)
      CHECK(UnblockSuspend());
  }
  void SuspendDone(const base::TimeDelta& sleep_duration) override {
    num_suspend_done_++;
  }
  void DarkSuspendImminent() override {
    num_dark_suspend_imminent_++;
    if (should_block_suspend_) {
      block_suspend_token_ = base::UnguessableToken::Create();
      client_->BlockSuspend(block_suspend_token_, FROM_HERE.ToString());
    }
    if (run_unblock_suspend_immediately_)
      CHECK(UnblockSuspend());
  }
  void AmbientColorChanged(const int32_t color_temperature) override {
    ambient_color_temperature_ = color_temperature;
  }

 private:
  PowerManagerClient* client_;  // Not owned.

  // Number of times SuspendImminent(), SuspendDone(), and DarkSuspendImminent()
  // have been called.
  int num_suspend_imminent_ = 0;
  int num_suspend_done_ = 0;
  int num_dark_suspend_imminent_ = 0;

  // Should SuspendImminent() and DarkSuspendImminent() call |client_|'s
  // BlockSuspend() method?
  bool should_block_suspend_ = false;

  // Should SuspendImminent() and DarkSuspendImminent() unblock the suspend
  // synchronously after blocking itit? Only has an effect if
  // |should_block_suspend_| is true.
  bool run_unblock_suspend_immediately_ = false;

  // When non-empty, the token for the outstanding block-suspend registration.
  base::UnguessableToken block_suspend_token_;

  // Ambient color temperature
  int32_t ambient_color_temperature_ = 0;
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

// Stub implementation of PowerManagerClient::RenderProcessManagerDelegate.
class TestDelegate : public PowerManagerClient::RenderProcessManagerDelegate {
 public:
  explicit TestDelegate(PowerManagerClient* client) {
    client->SetRenderProcessManagerDelegate(weak_ptr_factory_.GetWeakPtr());
  }
  ~TestDelegate() override = default;

  int num_suspend_imminent() const { return num_suspend_imminent_; }
  int num_suspend_done() const { return num_suspend_done_; }

  // PowerManagerClient::RenderProcessManagerDelegate:
  void SuspendImminent() override { num_suspend_imminent_++; }
  void SuspendDone() override { num_suspend_done_++; }

 private:
  // Number of times SuspendImminent() and SuspendDone() have been called.
  int num_suspend_imminent_ = 0;
  int num_suspend_done_ = 0;

  base::WeakPtrFactory<TestDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class PowerManagerClientTest : public testing::Test {
 public:
  PowerManagerClientTest() = default;
  ~PowerManagerClientTest() override = default;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);

    proxy_ = new dbus::MockObjectProxy(
        bus_.get(), power_manager::kPowerManagerServiceName,
        dbus::ObjectPath(power_manager::kPowerManagerServicePath));

    // |client_|'s Init() method should request a proxy for communicating with
    // powerd.
    EXPECT_CALL(*bus_,
                GetObjectProxy(
                    power_manager::kPowerManagerServiceName,
                    dbus::ObjectPath(power_manager::kPowerManagerServicePath)))
        .WillRepeatedly(Return(proxy_.get()));

    EXPECT_CALL(*bus_, GetDBusTaskRunner())
        .WillRepeatedly(
            Return(task_environment_.GetMainThreadTaskRunner().get()));
    EXPECT_CALL(*bus_, GetOriginTaskRunner())
        .WillRepeatedly(
            Return(task_environment_.GetMainThreadTaskRunner().get()));

    // Save |client_|'s signal and name-owner-changed callbacks.
    EXPECT_CALL(*proxy_, DoConnectToSignal(kInterface, _, _, _))
        .WillRepeatedly(Invoke(this, &PowerManagerClientTest::ConnectToSignal));
    EXPECT_CALL(*proxy_, SetNameOwnerChangedCallback(_))
        .WillRepeatedly(SaveArg<0>(&name_owner_changed_callback_));

    // |client_|'s Init() method should register regular and dark suspend
    // delays.
    EXPECT_CALL(
        *proxy_,
        DoCallMethod(HasMember(power_manager::kRegisterSuspendDelayMethod), _,
                     _))
        .WillRepeatedly(
            Invoke(this, &PowerManagerClientTest::RegisterSuspendDelay));
    EXPECT_CALL(
        *proxy_,
        DoCallMethod(HasMember(power_manager::kRegisterDarkSuspendDelayMethod),
                     _, _))
        .WillRepeatedly(
            Invoke(this, &PowerManagerClientTest::RegisterSuspendDelay));
    // Init should also request a fresh power status.
    EXPECT_CALL(
        *proxy_,
        DoCallMethod(HasMember(power_manager::kGetPowerSupplyPropertiesMethod),
                     _, _));
    // Init will test for the presence of an ambient light sensor.
    EXPECT_CALL(
        *proxy_,
        DoCallMethod(HasMember(power_manager::kHasAmbientColorDeviceMethod), _,
                     _));

    PowerManagerClient::Initialize(bus_.get());
    client_ = PowerManagerClient::Get();

    // Execute callbacks posted by Init().
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { PowerManagerClient::Shutdown(); }

 protected:
  // Synchronously passes |signal| to |client_|'s handler, simulating the signal
  // being emitted by powerd.
  void EmitSignal(dbus::Signal* signal) {
    const std::string signal_name = signal->GetMember();
    const auto it = signal_callbacks_.find(signal_name);
    ASSERT_TRUE(it != signal_callbacks_.end())
        << "Client didn't register for signal " << signal_name;
    it->second.Run(signal);
  }

  // Passes a SuspendImminent or DarkSuspendImminent signal to |client_|.
  void EmitSuspendImminentSignal(const std::string& signal_name,
                                 int suspend_id) {
    power_manager::SuspendImminent proto;
    proto.set_suspend_id(suspend_id);
    proto.set_reason(power_manager::SuspendImminent_Reason_OTHER);
    dbus::Signal signal(kInterface, signal_name);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
    EmitSignal(&signal);
  }

  // Passes a SuspendDone signal to |client_|.
  void EmitSuspendDoneSignal(int suspend_id) {
    power_manager::SuspendDone proto;
    proto.set_suspend_id(suspend_id);
    dbus::Signal signal(kInterface, power_manager::kSuspendDoneSignal);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
    EmitSignal(&signal);
  }

  // Adds an expectation to |proxy_| for a HandleSuspendReadiness or
  // HandleDarkSuspendReadiness method call.
  void ExpectSuspendReadiness(const std::string& method_name,
                              int suspend_id,
                              int delay_id) {
    EXPECT_CALL(
        *proxy_.get(),
        DoCallMethod(IsSuspendReadiness(method_name, suspend_id, delay_id), _,
                     _));
  }

  // Arbitrary delay IDs returned to |client_|.
  static const int kSuspendDelayId = 100;
  static const int kDarkSuspendDelayId = 200;

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls to powerd.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  PowerManagerClient* client_ = nullptr;

  // Maps from powerd signal name to the corresponding callback provided by
  // |client_|.
  std::map<std::string, dbus::ObjectProxy::SignalCallback> signal_callbacks_;

  // Callback passed to |proxy_|'s SetNameOwnerChangedCallback() method.
  // TODO(derat): Test that |client_| handles powerd restarts.
  dbus::ObjectProxy::NameOwnerChangedCallback name_owner_changed_callback_;

 private:
  // Handles calls to |proxy_|'s ConnectToSignal() method.
  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    CHECK_EQ(interface_name, power_manager::kPowerManagerInterface);
    signal_callbacks_[signal_name] = signal_callback;

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*on_connected_callback), interface_name,
                       signal_name, true /* success */));
  }

  // Handles calls to |proxy_|'s CallMethod() method to register suspend delays.
  void RegisterSuspendDelay(dbus::MethodCall* method_call,
                            int timeout_ms,
                            dbus::ObjectProxy::ResponseCallback* callback) {
    power_manager::RegisterSuspendDelayReply proto;
    proto.set_delay_id(method_call->GetMember() ==
                               power_manager::kRegisterDarkSuspendDelayMethod
                           ? kDarkSuspendDelayId
                           : kSuspendDelayId);

    method_call->SetSerial(123);  // Arbitrary but needed by FromMethodCall().
    std::unique_ptr<dbus::Response> response(
        dbus::Response::FromMethodCall(method_call));
    CHECK(dbus::MessageWriter(response.get()).AppendProtoAsArrayOfBytes(proto));

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&RunResponseCallback, std::move(*callback),
                                  std::move(response)));
  }

  DISALLOW_COPY_AND_ASSIGN(PowerManagerClientTest);
};

// Tests that suspend readiness is reported immediately when there are no
// observers.
TEST_F(PowerManagerClientTest, ReportSuspendReadinessWithoutObservers) {
  const int kSuspendId = 1;
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EmitSuspendDoneSignal(kSuspendId);
}

// Tests that synchronous observers are notified about impending suspend
// attempts and completion.
TEST_F(PowerManagerClientTest, ReportSuspendReadinessWithoutCallbacks) {
  TestObserver observer_1(client_);
  TestObserver observer_2(client_);

  // Observers should be notified when suspend is imminent. Readiness should be
  // reported synchronously since GetSuspendReadinessCallback() hasn't been
  // called.
  const int kSuspendId = 1;
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_EQ(1, observer_1.num_suspend_imminent());
  EXPECT_EQ(0, observer_1.num_suspend_done());
  EXPECT_EQ(1, observer_2.num_suspend_imminent());
  EXPECT_EQ(0, observer_2.num_suspend_done());

  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, observer_1.num_suspend_imminent());
  EXPECT_EQ(1, observer_1.num_suspend_done());
  EXPECT_EQ(1, observer_2.num_suspend_imminent());
  EXPECT_EQ(1, observer_2.num_suspend_done());
}

// Tests that readiness is deferred until asynchronous observers have run their
// callbacks.
TEST_F(PowerManagerClientTest, ReportSuspendReadinessWithCallbacks) {
  TestObserver observer_1(client_);
  observer_1.set_should_block_suspend(true);
  TestObserver observer_2(client_);
  observer_2.set_should_block_suspend(true);
  TestObserver observer_3(client_);

  // When observers call GetSuspendReadinessCallback() from their
  // SuspendImminent() methods, the HandleSuspendReadiness method call should be
  // deferred until all callbacks are run.
  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_TRUE(observer_1.UnblockSuspend());
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EXPECT_TRUE(observer_2.UnblockSuspend());
  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, observer_1.num_suspend_done());
  EXPECT_EQ(1, observer_2.num_suspend_done());
}

// Tests that RenderProcessManagerDelegate is notified about suspend and resume
// in the common case where suspend readiness is reported.
TEST_F(PowerManagerClientTest, NotifyRenderProcessManagerDelegate) {
  TestDelegate delegate(client_);
  TestObserver observer(client_);
  observer.set_should_block_suspend(true);

  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_EQ(0, delegate.num_suspend_imminent());
  EXPECT_EQ(0, delegate.num_suspend_done());

  // The RenderProcessManagerDelegate should be notified that suspend is
  // imminent only after observers have reported readiness.
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EXPECT_TRUE(observer.UnblockSuspend());
  EXPECT_EQ(1, delegate.num_suspend_imminent());
  EXPECT_EQ(0, delegate.num_suspend_done());

  // The delegate should be notified immediately after the attempt completes.
  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, delegate.num_suspend_imminent());
  EXPECT_EQ(1, delegate.num_suspend_done());
}

// Tests that DarkSuspendImminent is handled in a manner similar to
// SuspendImminent.
TEST_F(PowerManagerClientTest, ReportDarkSuspendReadiness) {
  TestDelegate delegate(client_);
  TestObserver observer(client_);
  observer.set_should_block_suspend(true);

  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_EQ(1, observer.num_suspend_imminent());
  EXPECT_EQ(0, delegate.num_suspend_imminent());

  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EXPECT_TRUE(observer.UnblockSuspend());
  EXPECT_EQ(1, delegate.num_suspend_imminent());

  // The RenderProcessManagerDelegate shouldn't be notified about dark suspend
  // attempts.
  const int kDarkSuspendId = 5;
  EmitSuspendImminentSignal(kDarkSuspendImminent, kDarkSuspendId);
  EXPECT_EQ(1, observer.num_dark_suspend_imminent());
  EXPECT_EQ(1, delegate.num_suspend_imminent());
  EXPECT_EQ(0, delegate.num_suspend_done());

  ExpectSuspendReadiness(kHandleDarkSuspendReadiness, kDarkSuspendId,
                         kDarkSuspendDelayId);
  EXPECT_TRUE(observer.UnblockSuspend());
  EXPECT_EQ(0, delegate.num_suspend_done());

  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, observer.num_suspend_done());
  EXPECT_EQ(1, delegate.num_suspend_done());
}

// Tests the case where a SuspendDone signal is received while a readiness
// callback is still pending.
TEST_F(PowerManagerClientTest, SuspendCancelledWhileCallbackPending) {
  TestDelegate delegate(client_);
  TestObserver observer(client_);
  observer.set_should_block_suspend(true);

  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_EQ(1, observer.num_suspend_imminent());

  // If the suspend attempt completes (probably due to cancellation) before the
  // observer has run its readiness callback, the observer (but not the
  // delegate, which hasn't been notified about suspend being imminent yet)
  // should be notified about completion.
  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, observer.num_suspend_done());
  EXPECT_EQ(0, delegate.num_suspend_done());

  // Ensure that the delegate doesn't receive late notification of suspend being
  // imminent if the readiness callback runs at this point, since that would
  // leave the renderers in a frozen state (http://crbug.com/646912). There's an
  // implicit expectation that powerd doesn't get notified about readiness here,
  // too.
  EXPECT_TRUE(observer.UnblockSuspend());
  EXPECT_EQ(0, delegate.num_suspend_imminent());
  EXPECT_EQ(0, delegate.num_suspend_done());
}

// Tests the case where a SuspendDone signal is received while a dark suspend
// readiness callback is still pending.
TEST_F(PowerManagerClientTest, SuspendDoneWhileDarkSuspendCallbackPending) {
  TestDelegate delegate(client_);
  TestObserver observer(client_);
  observer.set_should_block_suspend(true);

  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EXPECT_TRUE(observer.UnblockSuspend());
  EXPECT_EQ(1, delegate.num_suspend_imminent());

  const int kDarkSuspendId = 5;
  EmitSuspendImminentSignal(kDarkSuspendImminent, kDarkSuspendId);
  EXPECT_EQ(1, observer.num_dark_suspend_imminent());

  // The delegate should be notified if the attempt completes now.
  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, observer.num_suspend_done());
  EXPECT_EQ(1, delegate.num_suspend_done());

  // Dark suspend readiness shouldn't be reported even if the callback runs at
  // this point, since the suspend attempt is already done. The delegate also
  // shouldn't receive any more calls.
  EXPECT_TRUE(observer.UnblockSuspend());
  EXPECT_EQ(1, delegate.num_suspend_imminent());
  EXPECT_EQ(1, delegate.num_suspend_done());
}

// Tests the case where dark suspend is announced while readiness hasn't been
// reported for the initial regular suspend attempt.
TEST_F(PowerManagerClientTest, DarkSuspendImminentWhileCallbackPending) {
  TestDelegate delegate(client_);
  TestObserver observer(client_);
  observer.set_should_block_suspend(true);

  // Announce that suspend is imminent and grab, but don't run, the readiness
  // callback.
  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_EQ(1, observer.num_suspend_imminent());
  base::UnguessableToken regular_token = observer.block_suspend_token();

  // Before readiness is reported, announce that dark suspend is imminent.
  const int kDarkSuspendId = 1;
  EmitSuspendImminentSignal(kDarkSuspendImminent, kDarkSuspendId);
  EXPECT_EQ(1, observer.num_dark_suspend_imminent());
  base::UnguessableToken dark_token = observer.block_suspend_token();

  // Complete the suspend attempt and run both of the earlier callbacks. Neither
  // should result in readiness being reported.
  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, observer.num_suspend_done());
  client_->UnblockSuspend(regular_token);
  client_->UnblockSuspend(dark_token);
}

// Tests that PowerManagerClient handles a single observer that requests a
// suspend-readiness callback and then runs it synchronously from within
// SuspendImminent() instead of running it asynchronously:
// http://crosbug.com/p/58295
TEST_F(PowerManagerClientTest, SyncCallbackWithSingleObserver) {
  TestObserver observer(client_);
  observer.set_should_block_suspend(true);
  observer.set_run_unblock_suspend_immediately(true);

  const int kSuspendId = 1;
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EmitSuspendDoneSignal(kSuspendId);
}

// Tests the case where one observer reports suspend readiness by running its
// callback before a second observer even gets notified about the suspend
// attempt. We shouldn't report suspend readiness until the second observer has
// been notified and confirmed readiness.
TEST_F(PowerManagerClientTest, SyncCallbackWithMultipleObservers) {
  TestObserver observer1(client_);
  observer1.set_should_block_suspend(true);
  observer1.set_run_unblock_suspend_immediately(true);

  TestObserver observer2(client_);
  observer2.set_should_block_suspend(true);

  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EXPECT_TRUE(observer2.UnblockSuspend());
  EmitSuspendDoneSignal(kSuspendId);
}

// Tests that observers are notified about changes in ambient color temperature.
TEST_F(PowerManagerClientTest, ChangeAmbientColorTemperature) {
  TestObserver observer(client_);

  constexpr int32_t kTemperature = 6500;
  dbus::Signal signal(kInterface,
                      power_manager::kAmbientColorTemperatureChangedSignal);
  dbus::MessageWriter(&signal).AppendInt32(kTemperature);
  EmitSignal(&signal);

  EXPECT_EQ(kTemperature, observer.ambient_color_temperature());
}

}  // namespace chromeos
