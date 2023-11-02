// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_internals_constants.h"

namespace gcm_driver {

// Resource paths.
const char kGcmInternalsCSS[] = "gcm_internals.css";
const char kGcmInternalsJS[] = "gcm_internals.js";

// Message handlers.
const char kGetGcmInternalsInfo[] = "getGcmInternalsInfo";
const char kSetGcmInternalsInfo[] = "set-gcm-internals-info";
const char kSetGcmInternalsRecording[] = "setGcmInternalsRecording";

// GCM internal info.
const char kAndroidId[] = "androidId";
const char kAndroidSecret[] = "androidSecret";
const char kCheckinInfo[] = "checkinInfo";
const char kConnectionClientCreated[] = "connectionClientCreated";
const char kConnectionInfo[] = "connectionInfo";
const char kConnectionState[] = "connectionState";
const char kDeviceInfo[] = "deviceInfo";
const char kGcmClientCreated[] = "gcmClientCreated";
const char kGcmClientState[] = "gcmClientState";
const char kGcmEnabled[] = "gcmEnabled";
const char kIsRecording[] = "isRecording";
const char kLastCheckin[] = "lastCheckin";
const char kNextCheckin[] = "nextCheckin";
const char kProfileServiceCreated[] = "profileServiceCreated";
const char kReceiveInfo[] = "receiveInfo";
const char kRegisteredAppIds[] = "registeredAppIds";
const char kRegistrationInfo[] = "registrationInfo";
const char kResendQueueSize[] = "resendQueueSize";
const char kSendInfo[] = "sendInfo";
const char kSendQueueSize[] = "sendQueueSize";
const char kDecryptionFailureInfo[] = "decryptionFailureInfo";

}  // namespace gcm_driver
