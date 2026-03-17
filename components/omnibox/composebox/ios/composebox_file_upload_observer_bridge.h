// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_FILE_UPLOAD_OBSERVER_BRIDGE_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_FILE_UPLOAD_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "components/contextual_search/contextual_search_context_controller.h"

// Objective-C protocol for observing file upload status changes from
// ComposeboxQueryController.
@protocol ComposeboxFileUploadObserver <NSObject>
- (void)onContextUploadStatusChanged:(const base::UnguessableToken&)contextToken
                            mimeType:(lens::MimeType)mimeType
                 contextUploadStatus:
                     (contextual_search::ContextUploadStatus)contextUploadStatus
                           errorType:
                               (const std::optional<
                                   contextual_search::ContextUploadErrorType>&)
                                   errorType;
@end

// Bridge class that forwards file upload status changes from a C++
// ComposeboxQueryController to an Objective-C observer.
class ComposeboxFileUploadObserverBridge
    : public contextual_search::ContextualSearchContextController::
          FileUploadStatusObserver {
 public:
  ComposeboxFileUploadObserverBridge(
      id<ComposeboxFileUploadObserver> observer,
      contextual_search::ContextualSearchContextController* controller);
  ~ComposeboxFileUploadObserverBridge() override;

  // ComposeboxQueryController::FileUploadStatusObserver implementation.
  void OnContextUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus context_upload_status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type) override;

 private:
  __weak id<ComposeboxFileUploadObserver> observer_;
  base::ScopedObservation<contextual_search::ContextualSearchContextController,
                          contextual_search::ContextualSearchContextController::
                              FileUploadStatusObserver>
      observation_{this};
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_FILE_UPLOAD_OBSERVER_BRIDGE_H_
