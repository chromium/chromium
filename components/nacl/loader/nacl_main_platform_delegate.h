// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_MAIN_PLATFORM_DELEGATE_H_
#define COMPONENTS_NACL_LOADER_NACL_MAIN_PLATFORM_DELEGATE_H_

namespace content {
struct MainFunctionParams;
}

class NaClMainPlatformDelegate {
 public:
  NaClMainPlatformDelegate() = default;

  NaClMainPlatformDelegate(const NaClMainPlatformDelegate&) = delete;
  NaClMainPlatformDelegate& operator=(const NaClMainPlatformDelegate&) = delete;

  // Initiate Lockdown.
  void EnableSandbox(const content::MainFunctionParams& parameters);
};

#endif  // COMPONENTS_NACL_LOADER_NACL_MAIN_PLATFORM_DELEGATE_H_
