// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_util.h"

#include "base/time/time.h"

namespace breadcrumbs {

base::TimeTicks GetStartTime() {
  static base::TimeTicks start_time = base::TimeTicks::Now();
  return start_time;
}

}  // namespace breadcrumbs