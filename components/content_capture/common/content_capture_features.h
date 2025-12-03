// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_
#define COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_

#include "base/feature_list.h"

namespace content_capture::features {

// Enables sending content capture metadata (e.g. sensitivity score, language
// string, language confidence) to the data share service.
BASE_DECLARE_FEATURE(kContentCaptureSendMetadataForDataShare);

bool IsContentCaptureEnabled();

bool ShouldSendMetadataForDataShare();

int TaskInitialDelayInMilliseconds();

}  // namespace content_capture::features

#endif  // COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_
