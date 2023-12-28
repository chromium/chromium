// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/chaps/chaps_client.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

using ::testing::Invoke;
using ::testing::Return;

namespace ash {
namespace {

std::vector<uint8_t> GetIsolateCredential() {
  return std::vector<uint8_t>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

std::unique_ptr<dbus::Response> CreateResponse(uint32_t result_code) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendUint32(result_code);
  return response;
}

std::unique_ptr<dbus::Response> CreateResponse(uint64_t uint64_value,
                                               uint32_t result_code) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendUint64(uint64_value);
  writer.AppendUint32(result_code);
  return response;
}

std::unique_ptr<dbus::Response> CreateResponse(std::vector<uint8_t> bytes,
                                               uint32_t result_code) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(bytes);
  writer.AppendUint32(result_code);
  return response;
}

std::unique_ptr<dbus::Response> CreateResponse(std::vector<uint64_t> handles,
                                               uint32_t result_code) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());

  dbus::MessageWriter array_writer(nullptr);
  // See dbus::MessageWriter::OpenArray comment, "t" means uint64_t.
  writer.OpenArray("t", &array_writer);
  for (uint64_t handle : handles) {
    array_writer.AppendUint64(handle);
  }
  writer.CloseContainer(&array_writer);

  writer.AppendUint32(result_code);
  return response;
}

std::unique_ptr<dbus::Response> CreateResponse(uint64_t uint64_value,
                                               std::vector<uint8_t> bytes,
                                               uint32_t result_code) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendUint64(uint64_value);
  writer.AppendArrayOfBytes(bytes);
  writer.AppendUint32(result_code);
  return response;
}

std::unique_ptr<dbus::Response> CreateResponse(uint64_t handle0,
                                               uint64_t handle1,
                                               uint32_t result_code) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendUint64(handle0);
  writer.AppendUint64(handle1);
  writer.AppendUint32(result_code);
  return response;
}

class SessionChapsClientTest : public testing::Test {
 public:
  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = base::MakeRefCounted<dbus::MockBus>(options);

    std::string chaps_name = "org.chromium.Chaps";
    dbus::ObjectPath chaps_path = dbus::ObjectPath("/org/chromium/Chaps");
    proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(bus_.get(), chaps_name,
                                                         chaps_path);

    EXPECT_CALL(*bus_.get(), GetObjectProxy(chaps_name, chaps_path))
        .WillRepeatedly(Return(proxy_.get()));

    ChapsClient::Initialize(bus_.get());
    client_ = ChapsClient::Get();
  }

  void TearDown() override {
    client_ = nullptr;
    ChapsClient::Shutdown();
  }

 protected:
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;
  raw_ptr<ChapsClient> client_ = nullptr;
};

bool PopBool(dbus::MessageReader& reader) {
  bool result = false;
  if (!reader.PopBool(&result)) {
    ADD_FAILURE() << "Bad D-Bus request, bool not found";
  }
  return result;
}

uint64_t PopUint64(dbus::MessageReader& reader) {
  uint64_t result = 0;
  if (!reader.PopUint64(&result)) {
    ADD_FAILURE() << "Bad D-Bus request, uint64_t not found";
  }
  return result;
}

std::vector<uint8_t> PopByteArray(dbus::MessageReader& reader) {
  std::vector<uint8_t> result;
  dbus::MessageReader sub_reader(nullptr);
  if (!reader.PopArray(&sub_reader)) {
    ADD_FAILURE() << "Bad D-Bus request, array not found";
    return result;
  }

  uint8_t byte;
  while (sub_reader.HasMoreData()) {
    if (sub_reader.PopByte(&byte)) {
      result.push_back(byte);
    } else {
      ADD_FAILURE() << "Bad D-Bus request, byte not found";
      break;
    }
  }
  return result;
}

// Test that GetSlotList correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, GetSlotList) {
  bool token_present = true;
  std::vector<uint64_t> out_slots = {2, 3, 4, 5};
  uint32_t result_code = 33;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kGetSlotListMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopBool(reader), token_present);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(out_slots, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
  client_->GetSlotList(token_present, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<std::vector<uint64_t>>(), out_slots);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that GetMechanismList correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, GetMechanismList) {
  uint64_t slot_id = 11;
  std::vector<uint64_t> out_mechanisms = {2, 3, 4, 5};
  uint32_t result_code = 33;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kGetMechanismListMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), slot_id);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(out_mechanisms, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
  client_->GetMechanismList(slot_id, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<std::vector<uint64_t>>(), out_mechanisms);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that OpenSession correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, OpenSession) {
  uint64_t slot_id = 11;
  uint64_t flags = 22;
  uint64_t out_slot_id = 33;
  uint32_t result_code = 44;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kOpenSessionMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), slot_id);
    EXPECT_EQ(PopUint64(reader), flags);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(out_slot_id, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, uint32_t> waiter;
  client_->OpenSession(slot_id, flags, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint64_t>(), out_slot_id);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that CloseSession correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, CloseSession) {
  uint64_t session_id = 11;
  uint32_t result_code = 22;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kCloseSessionMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint32_t> waiter;
  client_->CloseSession(session_id, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that CreateObject correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, CreateObject) {
  uint64_t session_id = 11;
  std::vector<uint8_t> attributes = {2, 2, 2, 2};
  uint64_t out_handle = 33;
  uint32_t result_code = 44;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kCreateObjectMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopByteArray(reader), attributes);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(out_handle, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, uint32_t> waiter;
  client_->CreateObject(session_id, attributes, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint64_t>(), out_handle);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that DestroyObject correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, DestroyObject) {
  uint64_t session_id = 11;
  uint64_t object_handle = 22;
  uint32_t result_code = 33;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kDestroyObjectMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), object_handle);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint32_t> waiter;
  client_->DestroyObject(session_id, object_handle, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that GetAttributeValue correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, GetAttributeValue) {
  uint64_t session_id = 11;
  uint64_t object_handle = 22;
  std::vector<uint8_t> attributes_query = {3, 3, 3};
  std::vector<uint8_t> out_attributes = {4, 4, 4};
  uint32_t result_code = 55;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kGetAttributeValueMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), object_handle);
    EXPECT_EQ(PopByteArray(reader), attributes_query);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(out_attributes, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<const std::vector<uint8_t>&, uint32_t> waiter;
  client_->GetAttributeValue(session_id, object_handle, attributes_query,
                             waiter.GetCallback());
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), out_attributes);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that SetAttributeValue correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, SetAttributeValue) {
  uint64_t session_id = 11;
  uint64_t object_handle = 22;
  std::vector<uint8_t> attributes = {3, 3, 3};
  uint32_t result_code = 44;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kSetAttributeValueMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), object_handle);
    EXPECT_EQ(PopByteArray(reader), attributes);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint32_t> waiter;
  client_->SetAttributeValue(session_id, object_handle, attributes,
                             waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that FindObjectsInit correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, FindObjectsInit) {
  uint64_t session_id = 11;
  std::vector<uint8_t> attributes = {2, 2, 2};
  uint32_t result_code = 33;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kFindObjectsInitMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopByteArray(reader), attributes);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint32_t> waiter;
  client_->FindObjectsInit(session_id, attributes, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that FindObjects correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, FindObjects) {
  uint64_t session_id = 11;
  uint64_t max_object_count = 222222222;
  std::vector<uint64_t> out_handles = {3, 4, 5, 6, 7};
  uint32_t result_code = 44;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kFindObjectsMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), max_object_count);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(out_handles, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
  client_->FindObjects(session_id, max_object_count, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<std::vector<uint64_t>>(), out_handles);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that FindObjectsFinal correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, FindObjectsFinal) {
  uint64_t session_id = 11;
  uint32_t result_code = 22;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kFindObjectsFinalMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint32_t> waiter;
  client_->FindObjectsFinal(session_id, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that EncryptInit correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, EncryptInit) {
  uint64_t session_id = 11;
  uint64_t mechanism_type = 22;
  std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  uint64_t key_handle = 44;
  uint32_t result_code = 55;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kEncryptInitMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), mechanism_type);
    EXPECT_EQ(PopByteArray(reader), mechanism_parameter);
    EXPECT_EQ(PopUint64(reader), key_handle);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint32_t> waiter;
  client_->EncryptInit(session_id, mechanism_type, mechanism_parameter,
                       key_handle, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that Encrypt correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, Encrypt) {
  uint64_t session_id = 11;
  std::vector<uint8_t> data = {2, 2, 2};
  uint64_t max_out_length = 33;
  uint64_t actual_out_length = 44;
  std::vector<uint8_t> out_data = {5, 5, 5};
  uint32_t result_code = 66;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kEncryptMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopByteArray(reader), data);
    EXPECT_EQ(PopUint64(reader), max_out_length);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(actual_out_length, out_data, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
      waiter;
  client_->Encrypt(session_id, data, max_out_length, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint64_t>(), actual_out_length);
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), out_data);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that DecryptInit correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, DecryptInit) {
  uint64_t session_id = 11;
  uint64_t mechanism_type = 22;
  std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  uint64_t key_handle = 44;
  uint32_t result_code = 55;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kDecryptInitMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), mechanism_type);
    EXPECT_EQ(PopByteArray(reader), mechanism_parameter);
    EXPECT_EQ(PopUint64(reader), key_handle);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint32_t> waiter;
  client_->DecryptInit(session_id, mechanism_type, mechanism_parameter,
                       key_handle, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that Decrypt correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, Decrypt) {
  uint64_t session_id = 11;
  std::vector<uint8_t> data = {2, 2, 2};
  uint64_t max_out_length = 33;
  uint64_t actual_out_length = 44;
  std::vector<uint8_t> out_data = {5, 5, 5};
  uint32_t result_code = 66;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kDecryptMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopByteArray(reader), data);
    EXPECT_EQ(PopUint64(reader), max_out_length);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(actual_out_length, out_data, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
      waiter;
  client_->Decrypt(session_id, data, max_out_length, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint64_t>(), actual_out_length);
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), out_data);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that SignInit correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, SignInit) {
  uint64_t session_id = 11;
  uint64_t mechanism_type = 22;
  std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  uint64_t key_handle = 44;
  uint32_t result_code = 55;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kSignInitMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), mechanism_type);
    EXPECT_EQ(PopByteArray(reader), mechanism_parameter);
    EXPECT_EQ(PopUint64(reader), key_handle);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint32_t> waiter;
  client_->SignInit(session_id, mechanism_type, mechanism_parameter, key_handle,
                    waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that Sign correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, Sign) {
  uint64_t session_id = 11;
  std::vector<uint8_t> data = {2, 2, 2};
  uint64_t max_out_length = 33;
  uint64_t actual_out_length = 44;
  std::vector<uint8_t> out_data = {5, 5, 5};
  uint32_t result_code = 66;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kSignMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopByteArray(reader), data);
    EXPECT_EQ(PopUint64(reader), max_out_length);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(actual_out_length, out_data, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
      waiter;
  client_->Sign(session_id, data, max_out_length, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint64_t>(), actual_out_length);
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), out_data);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that GenerateKeyPair correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, GenerateKeyPair) {
  uint64_t session_id = 11;
  uint64_t mechanism_type = 22;
  std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  std::vector<uint8_t> public_attributes = {4, 4, 4};
  std::vector<uint8_t> private_attributes = {5, 5, 5};
  uint64_t public_key_handle = 66;
  uint64_t private_key_handle = 77;
  uint32_t result_code = 88;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kGenerateKeyPairMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), mechanism_type);
    EXPECT_EQ(PopByteArray(reader), mechanism_parameter);
    EXPECT_EQ(PopByteArray(reader), public_attributes);
    EXPECT_EQ(PopByteArray(reader), private_attributes);
    EXPECT_FALSE(reader.HasMoreData());

    auto response =
        CreateResponse(public_key_handle, private_key_handle, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, uint64_t, uint32_t> waiter;
  client_->GenerateKeyPair(session_id, mechanism_type, mechanism_parameter,
                           public_attributes, private_attributes,
                           waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
  EXPECT_EQ(waiter.Get<0>(), public_key_handle);
  EXPECT_EQ(waiter.Get<1>(), private_key_handle);
}

// Test that WrapKey correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, WrapKey) {
  uint64_t session_id = 11;
  uint64_t mechanism_type = 22;
  std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  uint64_t wrapping_key_handle = 44;
  uint64_t key_handle = 55;
  uint64_t max_out_length = 66;
  uint64_t actual_out_length = 77;
  std::vector<uint8_t> out_wrapped_key = {8, 8, 8};
  uint32_t result_code = 99;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kWrapKeyMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), mechanism_type);
    EXPECT_EQ(PopByteArray(reader), mechanism_parameter);
    EXPECT_EQ(PopUint64(reader), wrapping_key_handle);
    EXPECT_EQ(PopUint64(reader), key_handle);
    EXPECT_EQ(PopUint64(reader), max_out_length);
    EXPECT_FALSE(reader.HasMoreData());

    auto response =
        CreateResponse(actual_out_length, out_wrapped_key, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
      waiter;
  client_->WrapKey(session_id, mechanism_type, mechanism_parameter,
                   wrapping_key_handle, key_handle, max_out_length,
                   waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint64_t>(), actual_out_length);
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), out_wrapped_key);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that UnwrapKey correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, UnwrapKey) {
  uint64_t session_id = 11;
  uint64_t mechanism_type = 22;
  std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  uint64_t wrapping_key_handle = 44;
  std::vector<uint8_t> wrapped_key = {4, 4, 4};
  std::vector<uint8_t> attributes = {5, 5, 5};
  uint64_t out_key_handle = 77;
  uint32_t result_code = 88;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kUnwrapKeyMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), mechanism_type);
    EXPECT_EQ(PopByteArray(reader), mechanism_parameter);
    EXPECT_EQ(PopUint64(reader), wrapping_key_handle);
    EXPECT_EQ(PopByteArray(reader), wrapped_key);
    EXPECT_EQ(PopByteArray(reader), attributes);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(out_key_handle, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, uint32_t> waiter;
  client_->UnwrapKey(session_id, mechanism_type, mechanism_parameter,
                     wrapping_key_handle, wrapped_key, attributes,
                     waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint64_t>(), out_key_handle);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that DeriveKey correctly encodes and decodes dbus messages.
TEST_F(SessionChapsClientTest, DeriveKey) {
  uint64_t session_id = 11;
  uint64_t mechanism_type = 22;
  std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  uint64_t base_key_handle = 44;
  std::vector<uint8_t> attributes = {5, 5, 5};
  uint64_t out_key_handle = 77;
  uint32_t result_code = 88;

  auto fake_dbus = [&](dbus::MethodCall* method_call, int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(method_call->GetMember(), chaps::kDeriveKeyMethod);
    dbus::MessageReader reader(method_call);
    EXPECT_EQ(PopByteArray(reader), GetIsolateCredential());
    EXPECT_EQ(PopUint64(reader), session_id);
    EXPECT_EQ(PopUint64(reader), mechanism_type);
    EXPECT_EQ(PopByteArray(reader), mechanism_parameter);
    EXPECT_EQ(PopUint64(reader), base_key_handle);
    EXPECT_EQ(PopByteArray(reader), attributes);
    EXPECT_FALSE(reader.HasMoreData());

    auto response = CreateResponse(out_key_handle, result_code);
    return std::move(*callback).Run(response.get());
  };
  EXPECT_CALL(*proxy_.get(), DoCallMethod).WillOnce(Invoke(fake_dbus));

  base::test::TestFuture<uint64_t, uint32_t> waiter;
  client_->DeriveKey(session_id, mechanism_type, mechanism_parameter,
                     base_key_handle, attributes, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint64_t>(), out_key_handle);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

}  // namespace
}  // namespace ash
