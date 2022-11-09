// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_TYPES_H_
#define COMPONENTS_METAL_UTIL_TYPES_H_

#if __OBJC__
@protocol MTLDevice;
@protocol MTLSharedEvent;
#endif

namespace metal {

// Metal's API uses Objective-C types, which cannot be included in C++ sources.
// This file defines types that resolve to Metal types in Objective-C sources
// and can be included in C++ sources.
#if __OBJC__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
using MTLDevicePtr = id<MTLDevice>;
using MTLSharedEventPtr = id<MTLSharedEvent>;
#pragma clang diagnostic pop
#else
class MTLDeviceProtocol;
using MTLDevicePtr = MTLDeviceProtocol*;
class MTLSharedEventProtocol;
using MTLSharedEventPtr = MTLSharedEventProtocol*;
#endif

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_TYPES_H_
