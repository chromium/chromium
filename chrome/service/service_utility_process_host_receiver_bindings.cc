// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This exposes services in the browser to utility processes.

#include "chrome/service/service_utility_process_host.h"

#include "content/public/common/font_cache_dispatcher_win.h"
#include "content/public/common/font_cache_win.mojom.h"

void ServiceUtilityProcessHost::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<content::mojom::FontCacheWin>()) {
    content::FontCacheDispatcher::Create(std::move(r));
    return;
  }
}
