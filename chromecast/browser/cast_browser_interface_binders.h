// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_INTERFACE_BINDERS_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_INTERFACE_BINDERS_H_

#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
class RenderFrameHost;
}

namespace chromecast {
namespace shell {

void PopulateCastFrameBinders(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map);

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_INTERFACE_BINDERS_H_
