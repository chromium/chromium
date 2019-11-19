// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_prefetch_gcm_handler.h"

namespace offline_pages {

TestPrefetchGCMHandler::TestPrefetchGCMHandler() = default;
TestPrefetchGCMHandler::~TestPrefetchGCMHandler() = default;

gcm::GCMAppHandler* TestPrefetchGCMHandler::AsGCMAppHandler() {
  return nullptr;
}

std::string TestPrefetchGCMHandler::GetAppId() const {
  return "com.google.test.PrefetchAppId";
}

void TestPrefetchGCMHandler::SetService(PrefetchService* service) {}
}  // namespace offline_pages
