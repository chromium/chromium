// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/featured/featured_client.h"

#include <string>

#include <base/logging.h>
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/featured/fake_featured_client.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash::featured {

namespace {

FeaturedClient* g_instance = nullptr;

// Production implementation of FeaturedClient.
class FeaturedClientImpl : public FeaturedClient {
 public:
  FeaturedClientImpl() = default;

  FeaturedClientImpl(const FeaturedClient&) = delete;
  FeaturedClientImpl operator=(const FeaturedClient&) = delete;

  ~FeaturedClientImpl() override = default;

  void Init(dbus::Bus* const bus) {
    featured_service_proxy_ =
        bus->GetObjectProxy(::featured::kFeaturedServiceName,
                            dbus::ObjectPath(::featured::kFeaturedServicePath));
  }

  void HandleSeedFetchedResponse(
      base::OnceCallback<void(bool success)> callback,
      dbus::Response* response) {
    if (!response ||
        response->GetMessageType() != dbus::Message::MESSAGE_METHOD_RETURN) {
      LOG(WARNING) << "Received invalid response for HandleSeedFetched";
      std::move(callback).Run(false);
      return;
    }
    std::move(callback).Run(true);
  }

  void HandleSeedFetched(
      const ::featured::SeedDetails& safe_seed,
      base::OnceCallback<void(bool success)> callback) override {
    dbus::MethodCall method_call(::featured::kFeaturedInterface,
                                 "HandleSeedFetched");

    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(safe_seed);

    featured_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FeaturedClientImpl::HandleSeedFetchedResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> featured_service_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FeaturedClientImpl> weak_ptr_factory_{this};
};

}  // namespace

FeaturedClient::FeaturedClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FeaturedClient::~FeaturedClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void FeaturedClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new FeaturedClientImpl())->Init(bus);
}

// static
void FeaturedClient::InitializeFake() {
  new FakeFeaturedClient();
}

// static
void FeaturedClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
FeaturedClient* FeaturedClient::Get() {
  return g_instance;
}

}  // namespace ash::featured
