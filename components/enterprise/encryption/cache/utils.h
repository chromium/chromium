// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ENCRYPTION_CACHE_UTILS_H_
#define COMPONENTS_ENTERPRISE_ENCRYPTION_CACHE_UTILS_H_

class PrefService;

namespace enterprise_encryption {

// Whether the HTTP cache should be encrypted due to enterprise policy.
bool ShouldEncryptHttpCache(const PrefService* prefs);

}  // namespace enterprise_encryption

#endif  // COMPONENTS_ENTERPRISE_ENCRYPTION_CACHE_UTILS_H_
