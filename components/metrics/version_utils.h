// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_VERSION_UTILS_H_
#define COMPONENTS_METRICS_VERSION_UTILS_H_

#include <string>

#include "third_party/metrics_proto/system_profile.pb.h"

namespace version_info {
enum class Channel;
}

namespace metrics {

// Build a string including the Chrome app version, suffixed by "-64" on 64-bit
// platforms, and "-devel" on developer builds.
std::string GetVersionString();

// Translates version_info::Channel to the equivalent
// SystemProfileProto::Channel.
SystemProfileProto::Channel AsProtobufChannel(version_info::Channel channel);

// Gets Chrome's package name in Android Chrome, or an empty string on other
// platforms.
std::string GetAppPackageName();

}  // namespace metrics

#endif  // COMPONENTS_METRICS_VERSION_UTILS_H_
