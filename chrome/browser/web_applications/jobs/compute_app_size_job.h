// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_COMPUTE_APP_SIZE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_COMPUTE_APP_SIZE_JOB_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace web_app {

class ComputedAppSizeWithOrigin;
class WithAppResources;

// Abstract base class for jobs calculating the on-disk storage size
// for an installed web app.
class ComputeAppSizeJob {
 public:
  using ResultCallback =
      base::OnceCallback<void(std::optional<ComputedAppSizeWithOrigin> result)>;

  virtual ~ComputeAppSizeJob() = default;

  virtual void Start(WithAppResources* lock_with_app_resources,
                     ResultCallback callback) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_COMPUTE_APP_SIZE_JOB_H_
