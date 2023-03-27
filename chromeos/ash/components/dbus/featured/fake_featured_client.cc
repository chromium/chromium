// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/featured/fake_featured_client.h"

#include <string>

#include "base/check_op.h"
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

void FakeFeaturedClient::SetCallbackSuccess(bool success) {
  success_ = success;
}

void FakeFeaturedClient::HandleSeedFetched(
    const ::featured::SeedDetails& safe_seed,
    base::OnceCallback<void(bool success)> callback) {
  std::move(callback).Run(success_);
}

}  // namespace ash::featured
