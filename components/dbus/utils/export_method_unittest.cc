// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/export_method.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/dbus/utils/read_message.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::Unused;

namespace dbus_utils {

namespace {

const char kInterface[] = "org.chromium.TestInterface";
const char kMethod[] = "TestMethod";

class ExportMethodTest : public testing::Test {
 public:
  ExportMethodTest() = default;
  ~ExportMethodTest() override = default;

  void SetUp() override {
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    exported_object_ = base::MakeRefCounted<dbus::MockExportedObject>(
        bus_.get(), dbus::ObjectPath("/org/chromium/TestObject"));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockExportedObject> exported_object_;
};

ExportMethodResult<bool> TestMethodCallback(std::string s, int32_t i) {
  EXPECT_EQ(s, "hello");
  EXPECT_EQ(i, 42);
  return std::make_tuple(true);
}

}  // namespace

TEST_F(ExportMethodTest, Basic) {
  dbus::ExportedObject::MethodCallCallback method_call_callback;
  EXPECT_CALL(*exported_object_, ExportMethod(kInterface, kMethod, _, _))
      .WillOnce(
          [&](Unused, Unused, dbus::ExportedObject::MethodCallCallback callback,
              dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            method_call_callback = std::move(callback);
            std::move(on_exported_callback).Run(kInterface, kMethod, true);
          });

  ExportMethod<"si", "b">(exported_object_.get(), kInterface, kMethod,
                          base::BindRepeating(&TestMethodCallback),
                          base::DoNothing());

  ASSERT_TRUE(method_call_callback);

  dbus::MethodCall method_call(kInterface, kMethod);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("hello");
  writer.AppendInt32(42);

  bool response_sent = false;
  method_call_callback.Run(
      &method_call,
      base::BindLambdaForTesting([&](std::unique_ptr<dbus::Response> response) {
        response_sent = true;
        EXPECT_EQ(response->GetReplySerial(), 123U);
        auto result = internal::ReadMessage<std::tuple<bool>>(*response);
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(std::get<0>(*result));
      }));

  EXPECT_TRUE(response_sent);
}

TEST_F(ExportMethodTest, InvalidArgs) {
  dbus::ExportedObject::MethodCallCallback method_call_callback;
  EXPECT_CALL(*exported_object_, ExportMethod(kInterface, kMethod, _, _))
      .WillOnce(
          [&](Unused, Unused, dbus::ExportedObject::MethodCallCallback callback,
              dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            method_call_callback = std::move(callback);
            std::move(on_exported_callback).Run(kInterface, kMethod, true);
          });

  ExportMethod<"si", "b">(exported_object_.get(), kInterface, kMethod,
                          base::BindRepeating(&TestMethodCallback),
                          base::DoNothing());

  ASSERT_TRUE(method_call_callback);

  dbus::MethodCall method_call(kInterface, kMethod);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("hello");
  // Missing int32 argument.

  bool error_sent = false;
  method_call_callback.Run(
      &method_call,
      base::BindLambdaForTesting([&](std::unique_ptr<dbus::Response> response) {
        error_sent = true;
        EXPECT_EQ(response->GetMessageType(), dbus::Message::MESSAGE_ERROR);
        EXPECT_EQ(response->GetReplySerial(), 123U);
        EXPECT_EQ(
            static_cast<dbus::ErrorResponse*>(response.get())->GetErrorName(),
            DBUS_ERROR_INVALID_ARGS);
      }));

  EXPECT_TRUE(error_sent);
}

TEST_F(ExportMethodTest, Error) {
  dbus::ExportedObject::MethodCallCallback method_call_callback;
  EXPECT_CALL(*exported_object_, ExportMethod(kInterface, kMethod, _, _))
      .WillOnce(
          [&](Unused, Unused, dbus::ExportedObject::MethodCallCallback callback,
              dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            method_call_callback = std::move(callback);
            std::move(on_exported_callback).Run(kInterface, kMethod, true);
          });

  ExportMethod<"si", "b">(
      exported_object_.get(), kInterface, kMethod,
      base::BindRepeating(
          [](std::string s, int32_t i) -> ExportMethodResult<bool> {
            return base::unexpected(
                ExportMethodError{"org.chromium.Error", "Failed"});
          }),
      base::DoNothing());

  ASSERT_TRUE(method_call_callback);

  dbus::MethodCall method_call(kInterface, kMethod);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("hello");
  writer.AppendInt32(42);

  bool error_sent = false;
  method_call_callback.Run(
      &method_call,
      base::BindLambdaForTesting([&](std::unique_ptr<dbus::Response> response) {
        error_sent = true;
        EXPECT_EQ(response->GetMessageType(), dbus::Message::MESSAGE_ERROR);
        EXPECT_EQ(response->GetReplySerial(), 123U);
        auto* error_response =
            static_cast<dbus::ErrorResponse*>(response.get());
        EXPECT_EQ(error_response->GetErrorName(), "org.chromium.Error");
      }));

  EXPECT_TRUE(error_sent);
}

}  // namespace dbus_utils
