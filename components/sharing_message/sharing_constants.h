// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_CONSTANTS_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_CONSTANTS_H_

#include "base/time/time.h"
#include "net/base/backoff_entry.h"

// App ID linked to FCM messages for Sharing.
extern const char kSharingFCMAppID[];

// Sender ID for Sharing.
extern const char kSharingSenderID[];

// Time until we hide devices based on their last active timestamp.
extern const base::TimeDelta kSharingDeviceExpiration;

// Default time-to-live for sharing messages.
extern const base::TimeDelta kSharingMessageTTL;

// Default time-to-live for sharing ack messages.
extern const base::TimeDelta kSharingAckMessageTTL;

// Backoff policy for registration retry.
extern const net::BackoffEntry::Policy kRetryBackoffPolicy;

// Maximum number of devices to be shown in dialog and context menu.
extern const int kMaxDevicesShown;

// Command id for first device shown in submenu.
extern const int kSubMenuFirstDeviceCommandId;

// Command id for last device shown in submenu.
extern const int kSubMenuLastDeviceCommandId;

// The feature name prefix used in metrics name.
enum class SharingFeatureName {
  kUnknown,
  kClickToCall,
  kSharedClipboard,
  kSmsRemoteFetcher,
  kMaxValue = kSmsRemoteFetcher,
};

// The device platform that the user is sharing from/with.
enum class SharingDevicePlatform {
  kUnknown,
  kAndroid,
  kChromeOS,
  kIOS,
  kLinux,
  kMac,
  kWindows,
  kServer,
};

enum class SharingChannelType {
  kUnknown,
  kFcmVapid,
  kFcmSenderId,
  kServer,
  kWebRtc,
  kIosPush
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_CONSTANTS_H_
