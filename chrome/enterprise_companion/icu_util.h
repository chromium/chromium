// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ICU_UTIL_H_
#define CHROME_ENTERPRISE_COMPANION_ICU_UTIL_H_

namespace enterprise_companion {

// Performs best-effort initialization of ICU. This method may be invoked any
// number of times; initialization is attempted at most once per-process.
void InitializeICU();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ICU_UTIL_H_
