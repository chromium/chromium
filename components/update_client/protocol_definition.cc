// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_definition.h"

#include <string>

#include "base/values.h"

namespace update_client::protocol_request {

const char kProtocolVersion[] = "3.1";

OS::OS() = default;
OS::OS(OS&&) = default;
OS& OS::operator=(OS&&) = default;
OS::~OS() = default;

Updater::Updater() = default;
Updater::Updater(const Updater&) = default;
Updater::~Updater() = default;

UpdateCheck::UpdateCheck() = default;
UpdateCheck::~UpdateCheck() = default;

Data::Data() = default;
Data::Data(const Data& other) = default;
Data& Data::operator=(const Data&) = default;
Data::Data(const std::string& name,
           const std::string& install_data_index,
           const std::string& untrusted_data)
    : name(name),
      install_data_index(install_data_index),
      untrusted_data(untrusted_data) {}
Data::~Data() = default;

Ping::Ping() = default;
Ping::Ping(const Ping&) = default;
Ping::~Ping() = default;

App::App() = default;
App::App(App&&) = default;
App& App::operator=(App&&) = default;
App::~App() = default;

Request::Request() = default;
Request::Request(Request&&) = default;
Request& Request::operator=(Request&&) = default;
Request::~Request() = default;

}  // namespace update_client::protocol_request
