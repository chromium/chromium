// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client.h"

bool DevToolsClient::IsMainPage() {
  return GetRootClient() == this;
}
