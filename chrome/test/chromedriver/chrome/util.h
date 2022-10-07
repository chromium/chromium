// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_UTIL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_UTIL_H_

#include <string>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

Status SerializeAsJson(const base::Value::Dict& value, std::string* json);

Status SerializeAsJson(const base::Value& value, std::string* json);

Status SerializeAsJson(const std::string& value, std::string* json);

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_UTIL_H_
