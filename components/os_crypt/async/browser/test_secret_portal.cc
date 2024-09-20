// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/test_secret_portal.h"

#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/utils/name_has_owner.h"
#include "dbus/message.h"
#include "secret_portal_key_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

TestSecretPortal::TestSecretPortal(bool pre_test) : pre_test_(pre_test) {
  dbus::Bus::Options bus_options;
  bus_options.bus_type = dbus::Bus::SESSION;
  bus_options.connection_type = dbus::Bus::PRIVATE;
  bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);

  exported_object_ = bus_->GetExportedObject(
      dbus::ObjectPath(SecretPortalKeyProvider::kObjectPathSecret));

  EXPECT_TRUE(exported_object_->ExportMethodAndBlock(
      SecretPortalKeyProvider::kInterfaceSecret,
      SecretPortalKeyProvider::kMethodRetrieveSecret,
      base::BindRepeating(&TestSecretPortal::RetrieveSecret,
                          weak_ptr_factory_.GetWeakPtr())));
}

TestSecretPortal::~TestSecretPortal() {
  exported_object_ = nullptr;
  bus_->ShutdownAndBlock();
}

std::string TestSecretPortal::BusName() const {
  return bus_->GetConnectionName();
}

void TestSecretPortal::RetrieveSecret(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  base::ScopedFD write_fd;
  EXPECT_TRUE(reader.PopFileDescriptor(&write_fd));

  base::WriteFileDescriptor(write_fd.get(), "secret");
  write_fd.reset();

  dbus::ObjectPath response_path;
  dbus::MessageReader dict_reader(nullptr);
  EXPECT_TRUE(reader.PopArray(&dict_reader));
  std::optional<std::string> token;
  while (dict_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    EXPECT_TRUE(dict_reader.PopDictEntry(&dict_entry_reader));
    std::string key;
    std::string value;
    EXPECT_TRUE(dict_entry_reader.PopString(&key));
    if (key == "handle_token") {
      EXPECT_TRUE(dict_entry_reader.PopVariantOfString(&value));
      auto bus_name = method_call->GetSender();
      response_path = dbus::ObjectPath(
          base::nix::XdgDesktopPortalRequestPath(bus_name, value));
    }
    if (key == "token") {
      EXPECT_TRUE(dict_entry_reader.PopVariantOfString(&value));
      EXPECT_EQ(value, "the_token");
      token = value;
    }
  }
  auto* exported_response = bus_->GetExportedObject(response_path);

  EXPECT_TRUE(!pre_test_ || !token.has_value());

  auto response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(response_path);
  std::move(response_sender).Run(std::move(response));

  dbus::Signal signal(SecretPortalKeyProvider::kInterfaceRequest,
                      SecretPortalKeyProvider::kSignalResponse);
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendUint32(0);
  DbusDictionary dict;
  dict.Put("token", MakeDbusVariant(DbusString("the_token")));
  dict.Write(&signal_writer);
  exported_response->SendSignal(&signal);
}

}  // namespace os_crypt_async
