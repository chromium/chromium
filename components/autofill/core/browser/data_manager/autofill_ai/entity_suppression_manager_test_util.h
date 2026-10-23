// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_TEST_UTIL_H_

#include <memory>

namespace autofill {

class EntitySuppressionManager;

// Creates an `EntitySuppressionManager` for tests.
//
// It is backed by a real `EntitySuppressionSyncBridge`, but with an in-memory
// store and a mock change processor that always reports that Sync is tracking
// metadata. This lets tests suppress entities without Sync running. Nothing is
// sent to Sync, so it can't be used to test Sync behavior.
//
// Runs the current thread's tasks until the manager has loaded, so it's ready
// to use when returned.
std::unique_ptr<EntitySuppressionManager> CreateTestEntitySuppressionManager();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_TEST_UTIL_H_
