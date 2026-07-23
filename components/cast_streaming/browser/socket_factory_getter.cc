// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/public/socket_factory_getter.h"

#include <utility>

namespace cast_streaming::SocketFactoryGetter {

void Set(openscreen_platform::SocketFactoryGetter::Callback getter) {
  openscreen_platform::SocketFactoryGetter::Set(std::move(getter));
}

void Clear() {
  openscreen_platform::SocketFactoryGetter::Clear();
}

bool IsSet() {
  return openscreen_platform::SocketFactoryGetter::IsSet();
}

}  // namespace cast_streaming::SocketFactoryGetter
