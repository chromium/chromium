// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/featured/fake_featured_client.h"

#include "base/check_op.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "dbus/object_proxy.h"

namespace ash::featured {

namespace {

// Used to track the fake instance, mirrors the instance in the base class.
FakeFeaturedClient* g_instance = nullptr;

}  // namespace

FakeFeaturedClient::FakeFeaturedClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeFeaturedClient::~FakeFeaturedClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeFeaturedClient* FakeFeaturedClient::Get() {
  return g_instance;
}

void FakeFeaturedClient::HandleSeedFetched(
    const ::featured::SeedDetails& safe_seed,
    base::OnceCallback<void(bool success)> callback) {
  handle_seed_fetched_attempts_++;

  if (responses_.empty()) {
    LOG(ERROR) << "Insufficient amount of responses added. Call AddResponse to "
                  "add expected response to invoke the callback with";
    std::move(callback).Run(false);
    return;
  }

  bool success = responses_.front();
  responses_.pop();
  std::move(callback).Run(success);
}

void FakeFeaturedClient::AddResponse(bool success) {
  responses_.push(success);
}

}  // namespace ash::featured
