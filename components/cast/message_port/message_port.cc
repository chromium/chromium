// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/message_port.h"

namespace cast_api_bindings {

MessagePort::Receiver::~Receiver() = default;
MessagePort::~MessagePort() = default;

}  // namespace cast_api_bindings