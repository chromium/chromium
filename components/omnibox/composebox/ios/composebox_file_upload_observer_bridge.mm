// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"

#import "base/check.h"

ComposeboxFileUploadObserverBridge::ComposeboxFileUploadObserverBridge(
    id<ComposeboxFileUploadObserver> observer,
    contextual_search::ContextualSearchContextController* controller)
    : observer_(observer) {
  DCHECK(observer_);
  observation_.Observe(controller);
}

ComposeboxFileUploadObserverBridge::~ComposeboxFileUploadObserverBridge() =
    default;

void ComposeboxFileUploadObserverBridge::OnContextUploadStatusChanged(
    const base::UnguessableToken& context_token,
    lens::MimeType mime_type,
    contextual_search::ContextUploadStatus context_upload_status,
    const std::optional<contextual_search::ContextUploadErrorType>&
        error_type) {
  [observer_ onContextUploadStatusChanged:context_token
                                 mimeType:mime_type
                      contextUploadStatus:context_upload_status
                                errorType:error_type];
}
