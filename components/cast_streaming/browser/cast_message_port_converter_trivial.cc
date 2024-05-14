// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_message_port_converter.h"

#include "base/notreached.h"

namespace cast_streaming {

std::unique_ptr<CastMessagePortConverter> CastMessagePortConverter::Create(
    ReceiverSession::MessagePortProvider message_port_provider,
    base::OnceClosure on_close) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace cast_streaming
