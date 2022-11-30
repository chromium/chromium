// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_INFO_CHANNEL_H_
#define COMPONENTS_VERSION_INFO_CHANNEL_H_

namespace version_info {

// The possible channels for an installation, from most fun to most stable.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.version_info
enum class Channel {
  UNKNOWN = 0,
  // DEFAULT is an alias for UNKNOWN because the build files use DEFAULT but the
  // code uses UNKNOWN. TODO(paulmiller): Combine DEFAULT & UNKNOWN.
  DEFAULT = UNKNOWN,
  CANARY = 1,
  DEV = 2,
  BETA = 3,
  STABLE = 4,
};

}  // namespace version_info

#endif  // COMPONENTS_VERSION_INFO_CHANNEL_H_
