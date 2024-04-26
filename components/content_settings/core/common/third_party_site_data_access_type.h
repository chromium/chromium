// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_THIRD_PARTY_SITE_DATA_ACCESS_TYPE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_THIRD_PARTY_SITE_DATA_ACCESS_TYPE_H_

// Represents the state of site data accesses from third-party sites.
// Keep in sync with CookieControlsSiteDataAccessType in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/40064612): Extend this to capture sites which only accessed
// partitioned cookies and storage.
enum class ThirdPartySiteDataAccessType {
  kAnyBlockedThirdPartySiteAccesses = 0,
  kAnyAllowedThirdPartySiteAccesses = 1,
  kNoThirdPartySiteAccesses = 2,

  kMaxValue = kNoThirdPartySiteAccesses
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_THIRD_PARTY_SITE_DATA_ACCESS_TYPE_H_
