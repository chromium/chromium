// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/persistence/test_site_data_reader.h"

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/public/persistence/site_data/feature_usage.h"

namespace performance_manager::testing {

namespace {

SiteFeatureUsage UsageFromOptionalBool(std::optional<bool> feature) {
  if (feature.has_value()) {
    return feature.value() ? SiteFeatureUsage::kSiteFeatureInUse
                           : SiteFeatureUsage::kSiteFeatureNotInUse;
  }
  return SiteFeatureUsage::kSiteFeatureUsageUnknown;
}

}  // namespace

SimpleTestSiteDataReader::SimpleTestSiteDataReader(
    const BackgroundFeatureUsage& usage)
    : background_usage_(usage) {}

SimpleTestSiteDataReader::~SimpleTestSiteDataReader() = default;

SiteFeatureUsage SimpleTestSiteDataReader::UpdatesFaviconInBackground() const {
  return UsageFromOptionalBool(background_usage_.updates_favicon);
}

SiteFeatureUsage SimpleTestSiteDataReader::UpdatesTitleInBackground() const {
  return UsageFromOptionalBool(background_usage_.updates_title);
}
SiteFeatureUsage SimpleTestSiteDataReader::UsesAudioInBackground() const {
  return UsageFromOptionalBool(background_usage_.uses_audio);
}

bool SimpleTestSiteDataReader::DataLoaded() const {
  return true;
}

void SimpleTestSiteDataReader::RegisterDataLoadedCallback(
    base::OnceClosure&& callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

}  // namespace performance_manager::testing
