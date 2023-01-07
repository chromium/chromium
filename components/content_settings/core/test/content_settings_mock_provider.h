// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_MOCK_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_MOCK_PROVIDER_H_

#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

// The class MockProvider is a mock for a non default content settings provider.
class MockProvider : public ObservableProvider {
 public:
  MockProvider();
  explicit MockProvider(bool read_only);

  MockProvider(const MockProvider&) = delete;
  MockProvider& operator=(const MockProvider&) = delete;

  ~MockProvider() override;

  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& requesting_url_pattern,
      const ContentSettingsPattern& embedding_url_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const ContentSettingConstraints& constraint = {}) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override {
  }

  void ShutdownOnUIThread() override;

  void set_read_only(bool read_only) { read_only_ = read_only; }

  bool read_only() const { return read_only_; }

 private:
  OriginIdentifierValueMap value_map_;
  bool read_only_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_MOCK_PROVIDER_H_
