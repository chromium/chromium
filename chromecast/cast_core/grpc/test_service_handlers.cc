// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/test_service_handlers.h"

namespace cast {
namespace utils {

const char SimpleServiceHandler::kSimpleCall[] = "SimpleCall";

const char ServerStreamingServiceHandler::kStreamingCall[] = "StreamingCall";

}  // namespace utils
}  // namespace cast
