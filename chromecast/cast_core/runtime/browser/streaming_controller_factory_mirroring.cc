// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chromecast/cast_core/runtime/browser/streaming_controller_base.h"
#include "chromecast/cast_core/runtime/browser/streaming_controller_mirroring.h"
#include "components/cast/message_port/message_port.h"

namespace chromecast {

// static
std::unique_ptr<StreamingController> StreamingControllerBase::Create(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    content::WebContents* web_contents) {
  return std::make_unique<StreamingControllerMirroring>(std::move(message_port),
                                                        web_contents);
}

}  // namespace chromecast
