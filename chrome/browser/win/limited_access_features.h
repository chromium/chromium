// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_LIMITED_ACCESS_FEATURES_H_
#define CHROME_BROWSER_WIN_LIMITED_ACCESS_FEATURES_H_

#include <string>

// Function to unlock a Windows Limited Access Feature.
// Limited Access Features are Windows platform features which require
// specific approval from Microsoft to be used in an application. Using them
// requires a specific feature ID `feature` and use token `token`.
bool TryToUnlockLimitedAccessFeature(const std::wstring& feature,
                                     const std::wstring& token);

#endif  // CHROME_BROWSER_WIN_LIMITED_ACCESS_FEATURES_H_
