// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/network_context.h"

using network::NetworkContextGetter;

namespace openscreen_platform {
namespace {

static NetworkContextGetter* GetInstance() {
  static NetworkContextGetter* getter = new NetworkContextGetter();
  return getter;
}

}  // namespace

void SetNetworkContextGetter(NetworkContextGetter network_context_getter) {
  NetworkContextGetter* getter = GetInstance();
  DCHECK(getter->is_null() || network_context_getter.is_null());
  *getter = std::move(network_context_getter);
}

void ClearNetworkContextGetter() {
  NetworkContextGetter* getter = GetInstance();
  getter->Reset();
}

bool HasNetworkContextGetter() {
  return !GetInstance()->is_null();
}

network::mojom::NetworkContext* GetNetworkContext() {
  NetworkContextGetter* getter = GetInstance();
  if (getter->is_null()) {
    return nullptr;
  }
  return getter->Run();
}

}  // namespace openscreen_platform
