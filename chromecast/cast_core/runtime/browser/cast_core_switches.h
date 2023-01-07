// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_CORE_SWITCHES_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_CORE_SWITCHES_H_

namespace chromecast {

// Enables insecure content in Cast Web Runtime. This unblocks MSPs that serve
// content from HTTP sources, like Amazon Prime.
constexpr char kAllowRunningInsecureContentInRuntime[] =
    "allow-running-insecure-content";

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_CORE_SWITCHES_H_
