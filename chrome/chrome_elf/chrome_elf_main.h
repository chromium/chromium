// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_CHROME_ELF_MAIN_H_
#define CHROME_CHROME_ELF_CHROME_ELF_MAIN_H_

// These functions are the cross-module import interface to chrome_elf.dll.
// It is used by chrome.exe, chrome.dll and other clients of chrome_elf.
// In tests, these functions are stubbed by implementations in
// chrome_elf_test_stubs.cc.
extern "C" {

void DumpProcessWithoutCrash();

// Returns true if |user_data_dir| or |invalid_data_dir| contain data.
// This should always be the case in non-test builds.
bool GetUserDataDirectoryThunk(wchar_t* user_data_dir,
                               size_t user_data_dir_length,
                               wchar_t* invalid_user_data_dir,
                               size_t invalid_user_data_dir_length);
// This function is a temporary workaround for https://crbug.com/655788. We
// need to come up with a better way to initialize crash reporting that can
// happen inside DllMain().
void SignalInitializeCrashReporting();
void SignalChromeElf();

// Sets the metrics client ID in crash keys.
void SetMetricsClientId(const char* client_id);

}  // extern "C"

#endif  // CHROME_CHROME_ELF_CHROME_ELF_MAIN_H_
