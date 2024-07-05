// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_URL_REQUEST_CLOSE_SOURCE_H_
#define COMPONENTS_CRONET_ANDROID_URL_REQUEST_CLOSE_SOURCE_H_

#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
namespace cronet {

// Do not reorder, this is bundled in both Cronet Impl and API which
// means that the versions might mismatch, Only add new entries.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum class ConnectionCloseSource {
  UNKNOWN = 0,
  PEER = 1,
  SELF = 2,
};

// Converts net error initiator source to counterparts accessible in Java.
ConnectionCloseSource NetSourceToJavaSource(quic::ConnectionCloseSource source);

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_URL_REQUEST_CLOSE_SOURCE_H_
