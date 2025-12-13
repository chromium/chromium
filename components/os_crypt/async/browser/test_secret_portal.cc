// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/test_secret_portal.h"

#include <map>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/message.h"
#include "secret_portal_key_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

TestSecretPortal::TestSecretPortal(bool pre_test) : pre_test_(pre_test) {
  dbus::Bus::Options bus_options;
  bus_options.bus_type = dbus::Bus::SESSION;
  bus_options.connection_type = dbus::Bus::PRIVATE;
  bus_ = base::MakeRefCounted<dbus::Bus>(std::move(bus_options));

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

  EXPECT_TRUE(base::WriteFileDescriptor(write_fd.get(), "secret"));
  write_fd.reset();

  auto options =
      dbus_utils::ReadValue<std::map<std::string, dbus_utils::Variant>>(reader);
  ASSERT_TRUE(options);

  auto handle_token_it = options->find("handle_token");
  ASSERT_NE(handle_token_it, options->end());
  auto handle_token = std::move(handle_token_it->second).Take<std::string>();
  ASSERT_TRUE(handle_token);
  dbus::ObjectPath response_path(base::nix::XdgDesktopPortalRequestPath(
      method_call->GetSender(), *handle_token));
  auto* exported_response = bus_->GetExportedObject(response_path);

  EXPECT_TRUE(!pre_test_ || !base::Contains(*options, "token"));

  auto response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(response_path);
  std::move(response_sender).Run(std::move(response));

  dbus::Signal signal(SecretPortalKeyProvider::kInterfaceRequest,
                      SecretPortalKeyProvider::kSignalResponse);
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendUint32(0);
  std::map<std::string, dbus_utils::Variant> results;
  results.emplace("token", dbus_utils::Variant::Wrap<"s">("the_token"));
  dbus_utils::WriteValue(signal_writer, results);
  exported_response->SendSignal(&signal);
}

}  // namespace os_crypt_async
