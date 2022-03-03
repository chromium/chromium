// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chromecast/cast_core/runtime/browser/streaming_controller_base.h"
#include "chromecast/cast_core/runtime/browser/streaming_controller_remoting.h"
#include "components/cast/message_port/message_port.h"

namespace chromecast {

// static
std::unique_ptr<StreamingController> StreamingControllerBase::Create(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    CastWebContents* cast_web_contents) {
  return std::make_unique<StreamingControllerRemoting>(std::move(message_port),
                                                       cast_web_contents);
}

}  // namespace chromecast
