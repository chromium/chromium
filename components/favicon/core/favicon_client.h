// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_CLIENT_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_CLIENT_H_

#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"

class GURL;

namespace favicon {

// This class abstracts operations that depend on the embedder's environment,
// e.g. Chrome.
class FaviconClient {
 public:
  FaviconClient() = default;

  FaviconClient(const FaviconClient&) = delete;
  FaviconClient& operator=(const FaviconClient&) = delete;

  virtual ~FaviconClient() = default;

  // Returns true if the specified URL is a native application page URL.
  // If this returns true the favicon for the page must be fetched using
  // GetFaviconForNativeApplicationURL().
  virtual bool IsNativeApplicationURL(const GURL& url) = 0;

  // Returns true if the specified URL is a Reader Mode page URL. If this
  // returns true the favicon for the page must be fetched using the original
  // (distillable) page URL.
  virtual bool IsReaderModeURL(const GURL& url) = 0;

  // Gets the original page URL from a Reader Mode page URL.
  virtual const GURL GetOriginalUrlFromReaderModeUrl(const GURL& url) = 0;

  // Requests the favicon for a native application page URL for the sizes
  // specified by `desired_sizes_in_pixel`. Returns a TaskId to use to cancel
  // the request using `tracker` or kBadTaskId if the request cannot be
  // scheduled. `callback` will be called with the favicon results.
  virtual base::CancelableTaskTracker::TaskId GetFaviconForNativeApplicationURL(
      const GURL& url,
      const std::vector<int>& desired_sizes_in_pixel,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_CLIENT_H_
