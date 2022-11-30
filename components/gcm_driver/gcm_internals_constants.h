// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_INTERNALS_CONSTANTS_H_
#define COMPONENTS_GCM_DRIVER_GCM_INTERNALS_CONSTANTS_H_

namespace gcm_driver {

// Resource paths.
// Must match the resource file names.
extern const char kGcmInternalsCSS[];
extern const char kGcmInternalsJS[];

// Message handlers.
// Must match the constants used in the resource files.
extern const char kGetGcmInternalsInfo[];
extern const char kSetGcmInternalsInfo[];
extern const char kSetGcmInternalsRecording[];

// GCM internal info.
// Must match the constants used in the resource files.
extern const char kAndroidId[];
extern const char kAndroidSecret[];
extern const char kCheckinInfo[];
extern const char kConnectionClientCreated[];
extern const char kConnectionInfo[];
extern const char kConnectionState[];
extern const char kDeviceInfo[];
extern const char kGcmClientCreated[];
extern const char kGcmClientState[];
extern const char kGcmEnabled[];
extern const char kIsRecording[];
extern const char kLastCheckin[];
extern const char kNextCheckin[];
extern const char kProfileServiceCreated[];
extern const char kReceiveInfo[];
extern const char kRegisteredAppIds[];
extern const char kRegistrationInfo[];
extern const char kResendQueueSize[];
extern const char kSendInfo[];
extern const char kSendQueueSize[];
extern const char kDecryptionFailureInfo[];

}  // namespace gcm_driver

#endif  // COMPONENTS_GCM_DRIVER_GCM_INTERNALS_CONSTANTS_H_
