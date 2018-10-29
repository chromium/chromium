// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_NOTIFIER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_NOTIFIER_H_

namespace offline_pages {

class SavePageRequest;

class RequestNotifier {
 public:
  // Status to return for failed notifications.
  // NOTE: for any changes to the enum, please also update related switch code
  // in RequestCoordinatorEventLogger.
  // GENERATED_JAVA_ENUM_PACKAGE:org.chromium.components.offlinepages
  // WARNING: You must update histograms.xml to match any changes made to
  // this enum (ie, histogram enum for OfflinePagesBackgroundSavePageResult).
  enum class BackgroundSavePageResult {
    SUCCESS = 0,
    LOADING_FAILURE = 1,
    LOADING_CANCELED = 2,
    FOREGROUND_CANCELED = 3,
    SAVE_FAILED = 4,
    EXPIRED = 5,
    RETRY_COUNT_EXCEEDED = 6,
    START_COUNT_EXCEEDED = 7,
    USER_CANCELED = 8,
    DOWNLOAD_THROTTLED = 9,
    kMaxValue = DOWNLOAD_THROTTLED,
  };

  virtual ~RequestNotifier() = default;

  // Notifies observers that |request| has been added.
  virtual void NotifyAdded(const SavePageRequest& request) = 0;

  // Notifies observers that |request| has been completed with |status|.
  virtual void NotifyCompleted(const SavePageRequest& request,
                               BackgroundSavePageResult status) = 0;

  // Notifies observers that |request| has been changed.
  virtual void NotifyChanged(const SavePageRequest& request) = 0;

  // Notifies observers of progress on |request|, which received
  // |received_bytes| at that point.
  virtual void NotifyNetworkProgress(const SavePageRequest& request,
                                     int64_t received_bytes) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_NOTIFIER_H_
