// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PLATFORM_INFO_H_
#define COMPONENTS_NACL_RENDERER_PLATFORM_INFO_H_

namespace nacl {
// Returns the kind of SFI sandbox implemented by NaCl on this
// platform.  See the implementation in platform_info.cc for possible
// values.
const char* GetSandboxArch();

// Returns the features for the system's processor. Used for PNaCl translation.
std::string GetCpuFeatures();
}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_PLATFORM_INFO_H_
