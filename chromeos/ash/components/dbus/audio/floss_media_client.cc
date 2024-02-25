// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/audio/floss_media_client.h"

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/audio/fake_floss_media_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kFlossServiceName[] = "org.chromium.bluetooth";
const char kFlossMediaInterface[] = "org.chromium.bluetooth.BluetoothMedia";

FlossMediaClient* g_instance = nullptr;

// The FlossMediaClient implementation used in production.
class FlossMediaClientImpl : public FlossMediaClient {
 public:
  explicit FlossMediaClientImpl(dbus::Bus* bus,
                                const dbus::ObjectPath& object_path) {
    floss_media_proxy_ = bus->GetObjectProxy(kFlossServiceName, object_path);
  }

  FlossMediaClientImpl(const FlossMediaClientImpl&) = delete;
  FlossMediaClientImpl& operator=(const FlossMediaClientImpl&) = delete;

  ~FlossMediaClientImpl() override = default;

  void SetPlayerPlaybackStatus(const std::string& playback_status) override {
    dbus::MethodCall method_call(kFlossMediaInterface,
                                 cras::kSetPlayerPlaybackStatus);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(playback_status);
    floss_media_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

  void SetPlayerIdentity(const std::string& identity) override {
    dbus::MethodCall method_call(kFlossMediaInterface,
                                 cras::kSetPlayerIdentity);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(identity);
    floss_media_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

  void SetPlayerPosition(const int64_t& position) override {
    dbus::MethodCall method_call(kFlossMediaInterface,
                                 cras::kSetPlayerPosition);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt64(position);
    floss_media_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

  void SetPlayerDuration(const int64_t& duration) override {
    dbus::MethodCall method_call(kFlossMediaInterface,
                                 cras::kSetPlayerMetadata);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter array_writer(nullptr);
    dbus::MessageWriter dict_entry_writer(nullptr);

    writer.OpenArray("{sv}", &array_writer);
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString("length");
    dict_entry_writer.AppendVariantOfInt64(duration);
    array_writer.CloseContainer(&dict_entry_writer);
    writer.CloseContainer(&array_writer);

    floss_media_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

  void SetPlayerMetadata(
      const std::map<std::string, std::string>& metadata) override {
    dbus::MethodCall method_call(kFlossMediaInterface,
                                 cras::kSetPlayerMetadata);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter array_writer(nullptr);
    dbus::MessageWriter dict_entry_writer(nullptr);

    writer.OpenArray("{sv}", &array_writer);

    for (auto& it : metadata) {
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(it.first);
      dict_entry_writer.AppendVariantOfString(it.second);
      array_writer.CloseContainer(&dict_entry_writer);
    }

    writer.CloseContainer(&array_writer);

    floss_media_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

 private:
  raw_ptr<dbus::ObjectProxy> floss_media_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FlossMediaClientImpl> weak_ptr_factory_{this};
};

}  // namespace

FlossMediaClient::FlossMediaClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FlossMediaClient::~FlossMediaClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void FlossMediaClient::Initialize(dbus::Bus* bus,
                                  const dbus::ObjectPath& object_path) {
  DCHECK(bus);
  new FlossMediaClientImpl(bus, object_path);
}

// static
void FlossMediaClient::InitializeFake() {
  new FakeFlossMediaClient();
}

// static
void FlossMediaClient::Shutdown() {
  delete g_instance;
}

// static
FlossMediaClient* FlossMediaClient::Get() {
  return g_instance;
}

}  // namespace ash
