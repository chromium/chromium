// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_CONNECTION_MAP_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_CONNECTION_MAP_H_

#include <string>
#include <unordered_map>

using SessionConnectionMap = std::unordered_map<std::string, std::vector<int>>;

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_CONNECTION_MAP_H_
