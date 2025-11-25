// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_
#define COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_

namespace content_capture::features {

bool IsContentCaptureEnabled();

int TaskInitialDelayInMilliseconds();

}  // namespace content_capture::features

#endif  // COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_
