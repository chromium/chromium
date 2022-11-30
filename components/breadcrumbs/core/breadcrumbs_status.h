// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_

namespace breadcrumbs {

// Returns true if breadcrumbs logging is enabled. Note that metrics consent
// must have been provided for crash reports (including attached breadcrumbs) to
// be uploaded, and for breadcrumbs to persist on disk.
bool IsEnabled();

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_
