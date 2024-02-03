// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/public/network_context_getter.h"

#include "components/openscreen_platform/network_context.h"

namespace cast_streaming {

void SetNetworkContextGetter(network::NetworkContextGetter getter) {
  openscreen_platform::SetNetworkContextGetter(std::move(getter));
}

void ClearNetworkContextGetter() {
  openscreen_platform::ClearNetworkContextGetter();
}

bool HasNetworkContextGetter() {
  return openscreen_platform::HasNetworkContextGetter();
}

}  // namespace cast_streaming
