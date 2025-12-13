// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/legacy_topics_cleaner.h"

#include <array>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace invalidation {

namespace {
// Legacy Sender IDs of FCM (Firebase Cloud Messaging).
constexpr std::array kLegacyProjectNumbers = {"1013309121859", "947318989803"};
}  // namespace

LegacyTopicsCleaner::LegacyTopicsCleaner(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<IdentityProvider> identity_provider,
    PrefService* pref_service)
    : identity_provider_(std::move(identity_provider)) {
  for (const auto* project_number : kLegacyProjectNumbers) {
    auto subscription_manager = PerUserTopicSubscriptionManager::Create(
        url_loader_factory.get(), identity_provider_.get(), pref_service,
        project_number);
    subscription_manager->Init();
    subscription_manager->UpdateSubscribedTopics(/*topics=*/{});
    topic_subscription_managers_.emplace_back(std::move(subscription_manager));
  }
}

LegacyTopicsCleaner::~LegacyTopicsCleaner() = default;

}  // namespace invalidation
