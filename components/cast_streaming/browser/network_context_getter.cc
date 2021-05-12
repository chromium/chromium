// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/public/network_context_getter.h"

#include "components/openscreen_platform/network_context.h"

namespace cast_streaming {

void SetNetworkContextGetter(NetworkContextGetter getter) {
  openscreen_platform::SetNetworkContextGetter(std::move(getter));
}

}  // namespace cast_streaming
