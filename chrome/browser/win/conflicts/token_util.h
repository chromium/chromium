// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_TOKEN_UTIL_H_
#define CHROME_BROWSER_WIN_CONFLICTS_TOKEN_UTIL_H_

// Returns true if the current thread token is part of the built-in
// Administrators group, which is a proxy for determining if the current user
// has administrator rights.
bool HasAdminRights();

#endif  // CHROME_BROWSER_WIN_CONFLICTS_TOKEN_UTIL_H_
