// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_definition.h"

#include "base/values.h"

namespace update_client {

namespace protocol_request {

OS::OS() = default;
OS::OS(OS&&) = default;
OS::~OS() = default;

Updater::Updater() = default;
Updater::Updater(const Updater&) = default;
Updater::~Updater() = default;

UpdateCheck::UpdateCheck() = default;
UpdateCheck::~UpdateCheck() = default;

Ping::Ping() = default;
Ping::Ping(const Ping&) = default;
Ping::~Ping() = default;

App::App() = default;
App::App(App&&) = default;
App::~App() = default;

Request::Request() = default;
Request::Request(Request&&) = default;
Request::~Request() = default;

}  // namespace protocol_request

}  // namespace update_client
