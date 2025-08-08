// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_FILE_UPLOAD_OBSERVER_BRIDGE_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_FILE_UPLOAD_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "components/omnibox/composebox/composebox_query_controller.h"

// Objective-C protocol for observing file upload status changes from
// ComposeboxQueryController.
@protocol ComposeboxFileUploadObserver <NSObject>
- (void)onFileUploadStatusChanged:(const base::UnguessableToken&)fileToken
                         mimeType:(lens::MimeType)mimeType
                 fileUploadStatus:(FileUploadStatus)fileUploadStatus
                        errorType:(const std::optional<FileUploadErrorType>&)
                                      errorType;
@end

// Bridge class that forwards file upload status changes from a C++
// ComposeboxQueryController to an Objective-C observer.
class ComposeboxFileUploadObserverBridge
    : public ComposeboxQueryController::FileUploadStatusObserver {
 public:
  ComposeboxFileUploadObserverBridge(id<ComposeboxFileUploadObserver> observer,
                                     ComposeboxQueryController* controller);
  ~ComposeboxFileUploadObserverBridge() override;

  // ComposeboxQueryController::FileUploadStatusObserver implementation.
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      FileUploadStatus file_upload_status,
      const std::optional<FileUploadErrorType>& error_type) override;

 private:
  __weak id<ComposeboxFileUploadObserver> observer_;
  base::ScopedObservation<ComposeboxQueryController,
                          ComposeboxQueryController::FileUploadStatusObserver>
      observation_{this};
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_FILE_UPLOAD_OBSERVER_BRIDGE_H_
