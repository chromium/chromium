// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/version.h"

#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_version.h"

namespace {

const int kSupportedBrowserVersion[] = {CHROME_VERSION};

}  // namespace

const int kSupportedBrowserMajorVersion = kSupportedBrowserVersion[0];
