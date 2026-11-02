// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_BRIDGE_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_BRIDGE_TEST_UTIL_H_

#include <memory>

namespace autofill {

class PersonalContextEntityMetadataSyncBridge;

// Creates a `PersonalContextEntityMetadataSyncBridge` for a
// `PersonalContextEntityMetadataManager` in tests.
//
// It has an in-memory store and a mock change processor that always reports
// that Sync is tracking metadata. This lets tests suppress entities without
// Sync running. Nothing is sent to Sync, so it can't be used to test Sync
// behavior.
//
// The bridge loads asynchronously, once the current thread runs its pending
// tasks. Tests that use it right away should wait until `IsLoaded()`.
std::unique_ptr<PersonalContextEntityMetadataSyncBridge>
CreateTestPersonalContextEntityMetadataSyncBridge();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_BRIDGE_TEST_UTIL_H_
