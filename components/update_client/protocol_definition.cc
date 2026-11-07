// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_definition.h"

#include <string>

#include "base/values.h"

namespace update_client::protocol_request {

OS::OS() = default;
OS::OS(OS&&) noexcept = default;
OS& OS::operator=(OS&&) noexcept = default;
OS::~OS() = default;

Updater::Updater() = default;
Updater::Updater(const Updater&) = default;
Updater& Updater::operator=(const Updater&) = default;
Updater::Updater(Updater&&) noexcept = default;
Updater& Updater::operator=(Updater&&) noexcept = default;
Updater::~Updater() = default;

UpdateCheck::UpdateCheck() = default;
UpdateCheck::UpdateCheck(const UpdateCheck&) = default;
UpdateCheck& UpdateCheck::operator=(const UpdateCheck&) = default;
UpdateCheck::UpdateCheck(UpdateCheck&&) noexcept = default;
UpdateCheck& UpdateCheck::operator=(UpdateCheck&&) noexcept = default;
UpdateCheck::~UpdateCheck() = default;

Data::Data() = default;
Data::Data(const Data& other) = default;
Data& Data::operator=(const Data&) = default;
Data::Data(Data&& other) noexcept = default;
Data& Data::operator=(Data&& other) noexcept = default;
Data::Data(const std::string& name,
           const std::string& install_data_index,
           const std::string& untrusted_data)
    : name(name),
      install_data_index(install_data_index),
      untrusted_data(untrusted_data) {}
Data::~Data() = default;

Ping::Ping() = default;
Ping::Ping(const Ping&) = default;
Ping& Ping::operator=(const Ping&) = default;
Ping::Ping(Ping&&) noexcept = default;
Ping& Ping::operator=(Ping&&) noexcept = default;
Ping::~Ping() = default;

App::App() = default;
App::App(App&&) noexcept = default;
App& App::operator=(App&&) noexcept = default;
App::~App() = default;

Request::Request() = default;
Request::Request(Request&&) noexcept = default;
Request& Request::operator=(Request&&) noexcept = default;
Request::~Request() = default;

}  // namespace update_client::protocol_request
