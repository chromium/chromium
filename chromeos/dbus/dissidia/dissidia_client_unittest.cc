// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dissidia/dissidia_client.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dissidia/dbus-constants.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

namespace {

// Test observer that records signals received.
class TestObserver : public DissidiaClient::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnProgress(int32_t percent, const std::string& stage) override {
    last_progress_percent_ = percent;
    last_progress_stage_ = stage;
    progress_count_++;
  }

  void OnCompleted(bool success,
                   dissidia::CompletedErrorCode error_code,
                   const std::string& message) override {
    last_completed_success_ = success;
    last_completed_error_code_ = error_code;
    last_completed_message_ = message;
    completed_count_++;
  }

  int progress_count_ = 0;
  int32_t last_progress_percent_ = -1;
  std::string last_progress_stage_;

  int completed_count_ = 0;
  bool last_completed_success_ = false;
  dissidia::CompletedErrorCode last_completed_error_code_ = dissidia::kSuccess;
  std::string last_completed_message_;
};

}  // namespace

class DissidiaClientTest : public testing::Test {
 public:
  DissidiaClientTest() = default;
  DissidiaClientTest(const DissidiaClientTest&) = delete;
  DissidiaClientTest& operator=(const DissidiaClientTest&) = delete;
  ~DissidiaClientTest() override = default;

  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    proxy_ = new dbus::MockObjectProxy(
        bus_.get(), dissidia::kDissidiaServiceName,
        dbus::ObjectPath(dissidia::kDissidiaServicePath));

    EXPECT_CALL(
        *bus_, GetObjectProxy(dissidia::kDissidiaServiceName,
                              dbus::ObjectPath(dissidia::kDissidiaServicePath)))
        .WillRepeatedly(Return(proxy_.get()));

    // Capture signal callbacks when ConnectToSignal is called.
    EXPECT_CALL(*proxy_, ConnectToSignal(dissidia::kDissidiaInterface, _, _, _))
        .WillRepeatedly(
            [this](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback on_connected) {
              if (signal_name == dissidia::kProgressSignal) {
                progress_signal_callback_ = std::move(signal_callback);
              } else if (signal_name == dissidia::kCompletedSignal) {
                completed_signal_callback_ = std::move(signal_callback);
              }
              std::move(on_connected)
                  .Run(interface_name, signal_name, /*success=*/true);
            });

    DissidiaClient::Initialize(bus_.get());
  }

  void TearDown() override { DissidiaClient::Shutdown(); }

  // Simulates a Progress signal from the daemon.
  void EmitProgressSignal(int32_t percent, const std::string& stage) {
    dbus::Signal signal(dissidia::kDissidiaInterface,
                        dissidia::kProgressSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendInt32(percent);
    writer.AppendString(stage);
    progress_signal_callback_.Run(&signal);
  }

  // Simulates a Completed signal from the daemon.
  void EmitCompletedSignal(bool success,
                           int32_t error_code,
                           const std::string& message) {
    dbus::Signal signal(dissidia::kDissidiaInterface,
                        dissidia::kCompletedSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendBool(success);
    writer.AppendInt32(error_code);
    writer.AppendString(message);
    completed_signal_callback_.Run(&signal);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  dbus::ObjectProxy::SignalCallback progress_signal_callback_;
  dbus::ObjectProxy::SignalCallback completed_signal_callback_;
};

TEST_F(DissidiaClientTest, PerformUpdate_Success) {
  EXPECT_CALL(*proxy_, CallMethod)
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        // Verify the target was passed correctly.
        dbus::MessageReader reader(method_call);
        std::string target;
        ASSERT_TRUE(reader.PopString(&target));
        EXPECT_EQ(target, "noctis");

        // Return a successful response: status=0 (kStarted), message.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendInt32(0);
        writer.AppendString("Update started successfully.");
        std::move(callback).Run(response.get());
      });

  dissidia::PerformUpdateStatus received_status;
  std::string received_message;

  DissidiaClient::Get()->PerformUpdate(
      "noctis",
      base::BindOnce(
          [](dissidia::PerformUpdateStatus* out_status,
             std::string* out_message, dissidia::PerformUpdateStatus status,
             const std::string& message) {
            *out_status = status;
            *out_message = message;
          },
          &received_status, &received_message));

  EXPECT_EQ(received_status, dissidia::kUpdateStarted);
  EXPECT_EQ(received_message, "Update started successfully.");
}

TEST_F(DissidiaClientTest, PerformUpdate_AlreadyOnImage) {
  EXPECT_CALL(*proxy_, CallMethod)
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendInt32(1);  // kAlreadyOnImage
        writer.AppendString("Already on the requested image.");
        std::move(callback).Run(response.get());
      });

  dissidia::PerformUpdateStatus received_status;
  std::string received_message;

  DissidiaClient::Get()->PerformUpdate(
      "selphie",
      base::BindOnce(
          [](dissidia::PerformUpdateStatus* out_status,
             std::string* out_message, dissidia::PerformUpdateStatus status,
             const std::string& message) {
            *out_status = status;
            *out_message = message;
          },
          &received_status, &received_message));

  EXPECT_EQ(received_status, dissidia::kAlreadyOnRequestedImage);
  EXPECT_EQ(received_message, "Already on the requested image.");
}

TEST_F(DissidiaClientTest, PerformUpdate_NoResponse) {
  EXPECT_CALL(*proxy_, CallMethod)
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        // Simulate no response from the daemon.
        std::move(callback).Run(nullptr);
      });

  dissidia::PerformUpdateStatus received_status;
  std::string received_message;

  DissidiaClient::Get()->PerformUpdate(
      "noctis",
      base::BindOnce(
          [](dissidia::PerformUpdateStatus* out_status,
             std::string* out_message, dissidia::PerformUpdateStatus status,
             const std::string& message) {
            *out_status = status;
            *out_message = message;
          },
          &received_status, &received_message));

  EXPECT_EQ(received_status, dissidia::kError);
  EXPECT_FALSE(received_message.empty());
}

TEST_F(DissidiaClientTest, PerformUpdate_MalformedResponse) {
  EXPECT_CALL(*proxy_, CallMethod)
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        // Return an empty response with no fields.
        std::move(callback).Run(dbus::Response::CreateEmpty().get());
      });

  dissidia::PerformUpdateStatus received_status;
  std::string received_message;

  DissidiaClient::Get()->PerformUpdate(
      "noctis",
      base::BindOnce(
          [](dissidia::PerformUpdateStatus* out_status,
             std::string* out_message, dissidia::PerformUpdateStatus status,
             const std::string& message) {
            *out_status = status;
            *out_message = message;
          },
          &received_status, &received_message));

  EXPECT_EQ(received_status, dissidia::kError);
}

TEST_F(DissidiaClientTest, ProgressSignal) {
  TestObserver observer;
  DissidiaClient::Get()->AddObserver(&observer);

  EmitProgressSignal(42, "download");

  EXPECT_EQ(observer.progress_count_, 1);
  EXPECT_EQ(observer.last_progress_percent_, 42);
  EXPECT_EQ(observer.last_progress_stage_, "download");

  EmitProgressSignal(85, "extract");

  EXPECT_EQ(observer.progress_count_, 2);
  EXPECT_EQ(observer.last_progress_percent_, 85);
  EXPECT_EQ(observer.last_progress_stage_, "extract");

  DissidiaClient::Get()->RemoveObserver(&observer);
}

TEST_F(DissidiaClientTest, CompletedSignal_Success) {
  TestObserver observer;
  DissidiaClient::Get()->AddObserver(&observer);

  EmitCompletedSignal(/*success=*/true, /*error_code=*/0, "Update complete");

  EXPECT_EQ(observer.completed_count_, 1);
  EXPECT_TRUE(observer.last_completed_success_);
  EXPECT_EQ(observer.last_completed_error_code_, dissidia::kSuccess);
  EXPECT_EQ(observer.last_completed_message_, "Update complete");

  DissidiaClient::Get()->RemoveObserver(&observer);
}

TEST_F(DissidiaClientTest, CompletedSignal_Failure) {
  TestObserver observer;
  DissidiaClient::Get()->AddObserver(&observer);

  EmitCompletedSignal(/*success=*/false, /*error_code=*/2, "Download failed");

  EXPECT_EQ(observer.completed_count_, 1);
  EXPECT_FALSE(observer.last_completed_success_);
  EXPECT_EQ(observer.last_completed_error_code_, dissidia::kDownloadFailed);
  EXPECT_EQ(observer.last_completed_message_, "Download failed");

  DissidiaClient::Get()->RemoveObserver(&observer);
}

TEST_F(DissidiaClientTest, MultipleObservers) {
  TestObserver observer1;
  TestObserver observer2;
  DissidiaClient::Get()->AddObserver(&observer1);
  DissidiaClient::Get()->AddObserver(&observer2);

  EmitProgressSignal(50, "verity");

  EXPECT_EQ(observer1.progress_count_, 1);
  EXPECT_EQ(observer2.progress_count_, 1);
  EXPECT_EQ(observer1.last_progress_percent_, 50);
  EXPECT_EQ(observer2.last_progress_percent_, 50);

  // Remove one observer and verify only the remaining one gets notified.
  DissidiaClient::Get()->RemoveObserver(&observer1);

  EmitProgressSignal(75, "finalize");

  EXPECT_EQ(observer1.progress_count_, 1);  // unchanged
  EXPECT_EQ(observer2.progress_count_, 2);

  DissidiaClient::Get()->RemoveObserver(&observer2);
}

TEST_F(DissidiaClientTest, RemovedObserverNotNotified) {
  TestObserver observer;
  DissidiaClient::Get()->AddObserver(&observer);
  DissidiaClient::Get()->RemoveObserver(&observer);

  EmitCompletedSignal(/*success=*/true, /*error_code=*/0, "Done");

  EXPECT_EQ(observer.completed_count_, 0);
}

}  // namespace chromeos
