// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_BROWSER_EXPOSED_GPU_INTERFACES_H_
#define CHROME_GPU_BROWSER_EXPOSED_GPU_INTERFACES_H_

namespace gpu {
struct GpuPreferences;
class GpuDriverBugWorkarounds;
}

namespace mojo {
class BinderMap;
}

class ChromeContentGpuClient;

// Populates a BinderMap with interfaces exposed by Chrome from the GPU process
// to the browser. The browser can bind these interfaces through
// |GpuProcessHost::BindReceiver()|.
void ExposeChromeGpuInterfacesToBrowser(
    ChromeContentGpuClient* client,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders);

#endif  // CHROME_GPU_BROWSER_EXPOSED_GPU_INTERFACES_H_
