// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/connect_to_signal.h"

#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace dbus_utils {
namespace {

constexpr const char kTestServiceName[] = "org.chromium.TestService";
constexpr const char kTestObjectPath[] = "/org/chromium/TestObject";
constexpr const char kTestInterface[] = "org.chromium.TestInterface";
constexpr const char kTestSignal[] = "TestSignal";

class ConnectToSignalTest : public testing::Test {
 public:
  void SetUp() override {
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

    EXPECT_CALL(*mock_bus_, GetObjectProxy(StrEq(kTestServiceName),
                                           dbus::ObjectPath(kTestObjectPath)))
        .WillRepeatedly(Return(mock_proxy_.get()));
  }

  void TearDown() override {
    mock_proxy_.reset();
    mock_bus_.reset();
  }

 protected:
  void ExpectConnectToSignal(bool success) {
    EXPECT_CALL(*mock_proxy_, ConnectToSignal(StrEq(kTestInterface),
                                              StrEq(kTestSignal), _, _))
        .WillOnce(
            [this, success](
                const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
              signal_callback_ = std::move(signal_callback);
              task_environment_.GetMainThreadTaskRunner()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(on_connected_callback),
                                 interface_name, signal_name, success));
            });
  }

  template <typename WriterCallback>
  void SimulateSignal(WriterCallback writer_callback) {
    dbus::Signal signal(kTestInterface, kTestSignal);
    dbus::MessageWriter writer(&signal);
    writer_callback(&writer);
    signal_callback_.Run(&signal);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  dbus::ObjectProxy::SignalCallback signal_callback_;
};

TEST_F(ConnectToSignalTest, ConnectSuccess) {
  ExpectConnectToSignal(true);

  base::test::TestFuture<const std::string&, const std::string&, bool> future;

  ConnectToSignal<"">(mock_proxy_.get(), kTestInterface, kTestSignal,
                      base::BindRepeating([](ConnectToSignalResult<>) {}),
                      future.GetCallback());

  EXPECT_TRUE(std::get<2>(future.Get()));
}

TEST_F(ConnectToSignalTest, ConnectFailure) {
  ExpectConnectToSignal(false);

  base::test::TestFuture<const std::string&, const std::string&, bool> future;

  ConnectToSignal<"">(mock_proxy_.get(), kTestInterface, kTestSignal,
                      base::BindRepeating([](ConnectToSignalResult<>) {}),
                      future.GetCallback());

  EXPECT_FALSE(std::get<2>(future.Get()));
}

TEST_F(ConnectToSignalTest, SignalSuccessNoArgs) {
  ExpectConnectToSignal(true);

  base::test::TestFuture<const std::string&, const std::string&, bool>
      connect_future;
  base::test::TestFuture<ConnectToSignalResult<>> signal_future;

  ConnectToSignal<"">(mock_proxy_.get(), kTestInterface, kTestSignal,
                      signal_future.GetRepeatingCallback(),
                      connect_future.GetCallback());

  EXPECT_TRUE(std::get<2>(connect_future.Get()));

  SimulateSignal([](dbus::MessageWriter* writer) {});

  EXPECT_TRUE(signal_future.Get().has_value());
}

TEST_F(ConnectToSignalTest, SignalSuccessWithArgs) {
  ExpectConnectToSignal(true);

  constexpr char kExpectedString[] = "test";
  constexpr int32_t kExpectedInt = 123;

  base::test::TestFuture<const std::string&, const std::string&, bool>
      connect_future;
  base::test::TestFuture<ConnectToSignalResultSig<"si">> signal_future;

  ConnectToSignal<"si">(mock_proxy_.get(), kTestInterface, kTestSignal,
                        signal_future.GetRepeatingCallback(),
                        connect_future.GetCallback());

  EXPECT_TRUE(std::get<2>(connect_future.Get()));

  SimulateSignal([&](dbus::MessageWriter* writer) {
    writer->AppendString(kExpectedString);
    writer->AppendInt32(kExpectedInt);
  });

  auto result = signal_future.Get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(result.value()), kExpectedString);
  EXPECT_EQ(std::get<1>(result.value()), kExpectedInt);
}

TEST_F(ConnectToSignalTest, InvalidResponseFormat_WrongType) {
  ExpectConnectToSignal(true);

  base::test::TestFuture<const std::string&, const std::string&, bool>
      connect_future;
  base::test::TestFuture<ConnectToSignalResultSig<"s">> signal_future;

  ConnectToSignal<"s">(mock_proxy_.get(), kTestInterface, kTestSignal,
                       signal_future.GetRepeatingCallback(),
                       connect_future.GetCallback());

  EXPECT_TRUE(std::get<2>(connect_future.Get()));

  // Send int instead of string.
  SimulateSignal(
      [&](dbus::MessageWriter* writer) { writer->AppendInt32(123); });

  auto result = signal_future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), MessageFormatError::kInvalidMessageFormat);
}

TEST_F(ConnectToSignalTest, InvalidResponseFormat_MissingData) {
  ExpectConnectToSignal(true);

  base::test::TestFuture<const std::string&, const std::string&, bool>
      connect_future;
  base::test::TestFuture<ConnectToSignalResultSig<"s">> signal_future;

  ConnectToSignal<"s">(mock_proxy_.get(), kTestInterface, kTestSignal,
                       signal_future.GetRepeatingCallback(),
                       connect_future.GetCallback());

  EXPECT_TRUE(std::get<2>(connect_future.Get()));

  // Send empty signal.
  SimulateSignal([](dbus::MessageWriter* writer) {});

  auto result = signal_future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), MessageFormatError::kInvalidMessageFormat);
}

TEST_F(ConnectToSignalTest, ExtraDataInResponse) {
  ExpectConnectToSignal(true);

  base::test::TestFuture<const std::string&, const std::string&, bool>
      connect_future;
  base::test::TestFuture<ConnectToSignalResult<>> signal_future;

  ConnectToSignal<"">(mock_proxy_.get(), kTestInterface, kTestSignal,
                      signal_future.GetRepeatingCallback(),
                      connect_future.GetCallback());

  EXPECT_TRUE(std::get<2>(connect_future.Get()));

  // Send extra string.
  SimulateSignal(
      [&](dbus::MessageWriter* writer) { writer->AppendString("extra"); });

  auto result = signal_future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), MessageFormatError::kExtraDataInMessage);
}

}  // namespace
}  // namespace dbus_utils
