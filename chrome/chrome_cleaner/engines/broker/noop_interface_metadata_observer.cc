// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/noop_interface_metadata_observer.h"

namespace chrome_cleaner {

NoOpInterfaceMetadataObserver::NoOpInterfaceMetadataObserver() = default;

NoOpInterfaceMetadataObserver::~NoOpInterfaceMetadataObserver() = default;

void NoOpInterfaceMetadataObserver::ObserveCall(
    const LogInformation& log_information,
    const std::map<std::string, std::string>& params) {}

void NoOpInterfaceMetadataObserver::ObserveCall(
    const LogInformation& log_information) {}

}  // namespace chrome_cleaner
