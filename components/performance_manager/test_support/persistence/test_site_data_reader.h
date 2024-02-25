// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERSISTENCE_TEST_SITE_DATA_READER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERSISTENCE_TEST_SITE_DATA_READER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/performance_manager/public/persistence/site_data/feature_usage.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"

namespace performance_manager {

namespace testing {

// A simple SiteDataReader that returns fixed data.
class SimpleTestSiteDataReader : public SiteDataReader {
 public:
  // Initial values for features that can be used in the background. For each
  // feature, the corresponding Updates*InBackground() method will return
  // kSiteFeatureUsageInUse on true, kSiteFeatureUsageNotInUse on false, and
  // kSiteFeatureUsageUnknown on nullopt.
  struct BackgroundFeatureUsage {
    std::optional<bool> updates_favicon;
    std::optional<bool> updates_title;
    std::optional<bool> uses_audio;
  };

  explicit SimpleTestSiteDataReader(const BackgroundFeatureUsage& usage = {});
  ~SimpleTestSiteDataReader() override;

  SimpleTestSiteDataReader(const SimpleTestSiteDataReader& other) = delete;
  SimpleTestSiteDataReader& operator=(const SimpleTestSiteDataReader& other) =
      delete;

  void set_updates_favicon_in_background(std::optional<bool> value) {
    background_usage_.updates_favicon = value;
  }

  void set_updates_title_in_background(std::optional<bool> value) {
    background_usage_.updates_title = value;
  }

  void set_uses_audio_in_background(std::optional<bool> value) {
    background_usage_.uses_audio = value;
  }

  // SiteDataReader:

  // Background feature usage accessors.
  SiteFeatureUsage UpdatesFaviconInBackground() const override;
  SiteFeatureUsage UpdatesTitleInBackground() const override;
  SiteFeatureUsage UsesAudioInBackground() const override;

  // Behave as if the site is fully loaded by returning `true`.
  bool DataLoaded() const override;

  // Behave as if the site is fully loaded by immediately invoking `callback`.
  void RegisterDataLoadedCallback(base::OnceClosure&& callback) override;

 private:
  BackgroundFeatureUsage background_usage_;
};

}  // namespace testing

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERSISTENCE_TEST_SITE_DATA_READER_H_
