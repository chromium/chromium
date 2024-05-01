// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_FUCHSIA_CREATE_WEB_MESSAGE_H_
#define COMPONENTS_CAST_MESSAGE_PORT_FUCHSIA_CREATE_WEB_MESSAGE_H_

#include <fuchsia/web/cpp/fidl.h>

#include <memory>
#include <string_view>

namespace cast_api_bindings {

class MessagePort;

}  // namespace cast_api_bindings

// Utility function for creating a fuchsia.web.WebMessage with the payload
// |message| and an optional transferred |port|.
fuchsia::web::WebMessage CreateWebMessage(
    std::string_view message,
    std::unique_ptr<cast_api_bindings::MessagePort> port);

#endif  // COMPONENTS_CAST_MESSAGE_PORT_FUCHSIA_CREATE_WEB_MESSAGE_H_
