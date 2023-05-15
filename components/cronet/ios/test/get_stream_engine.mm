// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include "components/cronet/ios/test/start_cronet.h"
#include "components/grpc_support/test/get_stream_engine.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface Cronet (ExposedForTesting)
+ (void)shutdownForTesting;
@end

namespace grpc_support {

stream_engine* GetTestStreamEngine(int port) {
  return [Cronet getGlobalEngine];
}

void StartTestStreamEngine(int port) {
  cronet::StartCronet(port);
}

void ShutdownTestStreamEngine() {
  [Cronet shutdownForTesting];
}

}  // namespace grpc_support
