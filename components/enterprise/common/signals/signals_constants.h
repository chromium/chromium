// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_COMMON_SIGNALS_SIGNALS_CONSTANTS_H_
#define COMPONENTS_ENTERPRISE_COMMON_SIGNALS_SIGNALS_CONSTANTS_H_

namespace enterprise_signals {

// Signal names can be used as keys to store/retrieve signal values from
// dictionaries.
namespace names {

extern const char kFileSystemInfo[];
extern const char kSettings[];
extern const char kAntiVirusInfo[];
extern const char kInstalledHotfixes[];

}  // namespace names

}  // namespace enterprise_signals

#endif  // COMPONENTS_ENTERPRISE_COMMON_SIGNALS_SIGNALS_CONSTANTS_H_
