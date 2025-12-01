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

void ComposeboxFileUploadObserverBridge::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {
  [observer_ onFileUploadStatusChanged:file_token
                              mimeType:mime_type
                      fileUploadStatus:file_upload_status
                             errorType:error_type];
}
