// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_SECURE_DLL_LOADING_H_
#define CHROME_CHROME_CLEANER_OS_SECURE_DLL_LOADING_H_

namespace chrome_cleaner {

// The name of an empty dll. This should only be used by tests.
extern const wchar_t kEmptyDll[];

// Prevent non-system or absolute path dlls from being loaded. This should help
// ensure we only ever load dlls that we completely mean to. This should be
// called as early as possible to reduce the window where dlls can be loaded
// without this protection.
bool EnableSecureDllLoading();

namespace testing {

// Attempt to load kEmptyDll. This should only be used by tests.
void LoadEmptyDLL();

}  // namespace testing

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_SECURE_DLL_LOADING_H_
