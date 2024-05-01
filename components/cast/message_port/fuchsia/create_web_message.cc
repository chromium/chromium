// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/fuchsia/create_web_message.h"

#include <string_view>

#include "base/fuchsia/mem_buffer_util.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"
#include "components/cast/message_port/message_port.h"

fuchsia::web::WebMessage CreateWebMessage(
    std::string_view message,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  fuchsia::web::WebMessage web_message;
  web_message.set_data(base::MemBufferFromString(message, "msg"));
  if (port) {
    fuchsia::web::OutgoingTransferable outgoing_transferable;
    outgoing_transferable.set_message_port(
        cast_api_bindings::MessagePortFuchsia::FromMessagePort(port.get())
            ->TakeServiceRequest());
    std::vector<fuchsia::web::OutgoingTransferable> outgoing_transferables;
    outgoing_transferables.push_back(std::move(outgoing_transferable));
    web_message.set_outgoing_transfer(std::move(outgoing_transferables));
  }
  return web_message;
}
