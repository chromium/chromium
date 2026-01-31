// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_UTILS_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_UTILS_H_

#include <memory>

namespace url_deduplication {
class URLDeduplicationHelper;
}  // namespace url_deduplication

namespace contextual_tasks {

// Creates a URLDeduplicationHelper for comparing URLs. Meant to be used for
// showing the list of distinct URLs associated with a context.
std::unique_ptr<url_deduplication::URLDeduplicationHelper>
CreateURLDeduplicationHelperForContextualTask();

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_UTILS_H_
