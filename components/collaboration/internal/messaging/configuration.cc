// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/configuration.h"

namespace collaboration::messaging {

MessagingBackendConfiguration::MessagingBackendConfiguration() = default;

MessagingBackendConfiguration::MessagingBackendConfiguration(
    const MessagingBackendConfiguration&) = default;

MessagingBackendConfiguration& MessagingBackendConfiguration::operator=(
    const MessagingBackendConfiguration&) = default;

MessagingBackendConfiguration::~MessagingBackendConfiguration() = default;

}  // namespace collaboration::messaging
