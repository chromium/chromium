// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

using ::testing::NiceMock;

MockTrustSafetySentimentService::MockTrustSafetySentimentService(
    Profile* profile)
    : TrustSafetySentimentService(profile) {}

MockTrustSafetySentimentService::~MockTrustSafetySentimentService() = default;

std::unique_ptr<KeyedService> BuildMockTrustSafetySentimentService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockTrustSafetySentimentService>>(
      static_cast<Profile*>(context));
}
