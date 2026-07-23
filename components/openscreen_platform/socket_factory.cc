// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/socket_factory.h"

#include "base/check.h"

namespace openscreen_platform {
namespace {

static SocketFactoryGetter::Callback* GetInstance() {
  static SocketFactoryGetter::Callback* getter =
      new SocketFactoryGetter::Callback();
  return getter;
}

}  // namespace

namespace SocketFactoryGetter {

void Set(Callback socket_factory_getter) {
  Callback* getter = GetInstance();
  CHECK(getter->is_null() || socket_factory_getter.is_null());
  *getter = std::move(socket_factory_getter);
}

void Clear() {
  Callback* getter = GetInstance();
  getter->Reset();
}

bool IsSet() {
  return !GetInstance()->is_null();
}

}  // namespace SocketFactoryGetter

network::mojom::SocketFactory* GetSocketFactory() {
  SocketFactoryGetter::Callback* getter = GetInstance();
  if (getter->is_null()) {
    return nullptr;
  }
  return getter->Run();
}

}  // namespace openscreen_platform
