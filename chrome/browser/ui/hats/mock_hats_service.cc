// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/mock_hats_service.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

using ::testing::NiceMock;

MockHatsService::MockHatsService(Profile* profile)
    : HatsServiceDesktop(profile) {}

MockHatsService::~MockHatsService() = default;

std::unique_ptr<KeyedService> BuildMockHatsService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockHatsService>>(
      static_cast<Profile*>(context));
}
