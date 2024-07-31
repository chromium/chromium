// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_SEND_MESSAGE_RESULT_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_SEND_MESSAGE_RESULT_H_

// Result of sending SharingMessage via sharing service.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please update the enum and suffix
// named SharingSendMessageResult in enums.xml and histograms.xml when adding
// a new entry here.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.sharing
enum class SharingSendMessageResult {
  kSuccessful = 0,
  kDeviceNotFound = 1,
  kNetworkError = 2,
  kPayloadTooLarge = 3,
  kAckTimeout = 4,
  kInternalError = 5,
  kEncryptionError = 6,
  kCommitTimeout = 7,
  kCancelled = 8,
  kMaxValue = kCancelled,
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_SEND_MESSAGE_RESULT_H_
