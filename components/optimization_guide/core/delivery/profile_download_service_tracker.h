// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace optimization_guide {

// Tracks the profiles and the background download service, so as to return the
// apt background download service to be used for model downloads. This connects
// the profile-specific background download service to the generic opt guide
// model delivery infrastructure.
class ProfileDownloadServiceTracker {
 public:
  virtual ~ProfileDownloadServiceTracker() = default;

  // Returns the background download service to be used for model download
  // purposes. Can be null when no valid profile exists.
  virtual download::BackgroundDownloadService*
  GetBackgroundDownloadService() = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_
