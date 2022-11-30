// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_client.h"

namespace gcm {

GCMClient::ChromeBuildInfo::ChromeBuildInfo()
    : platform(PLATFORM_UNSPECIFIED), channel(CHANNEL_UNKNOWN) {}

GCMClient::ChromeBuildInfo::~ChromeBuildInfo() = default;

GCMClient::SendErrorDetails::SendErrorDetails() : result(UNKNOWN_ERROR) {}

GCMClient::SendErrorDetails::SendErrorDetails(const SendErrorDetails& other) =
    default;

GCMClient::SendErrorDetails::~SendErrorDetails() = default;

GCMClient::GCMStatistics::GCMStatistics()
    : is_recording(false),
      gcm_client_created(false),
      connection_client_created(false),
      android_id(0u),
      android_secret(0u),
      send_queue_size(0),
      resend_queue_size(0) {}

GCMClient::GCMStatistics::GCMStatistics(const GCMStatistics& other) = default;

GCMClient::GCMStatistics::~GCMStatistics() = default;

GCMClient::GCMClient() = default;

GCMClient::~GCMClient() = default;

}  // namespace gcm
