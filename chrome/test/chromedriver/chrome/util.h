// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_UTIL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_UTIL_H_

#include <string>

namespace base {
class Value;
}

std::string SerializeValue(const base::Value* value);

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_UTIL_H_
