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
    std::move(callback).Run(false);
    return;
  }

  bool success = responses_.front();
  if (success) {
    // We only want to save the safe seed if the response (success) is valid.
    latest_safe_seed_ = safe_seed;
  }
  responses_.pop();
  std::move(callback).Run(success);
}

void FakeFeaturedClient::AddResponse(bool success) {
  responses_.push(success);
}

}  // namespace ash::featured
