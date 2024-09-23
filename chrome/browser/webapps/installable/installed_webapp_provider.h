// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_
#define CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_

#include <memory>
#include <vector>

#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

// PartitionKey is ignored by this provider because the content settings should
// apply across partitions.
class InstalledWebappProvider : public content_settings::ObservableProvider {
 public:
  // Although not used in the interface of this class, RuleList is the type for
  // the underlying data that this Provider holds.
  using RuleList = std::vector<std::pair<GURL, ContentSetting>>;

  InstalledWebappProvider();

  InstalledWebappProvider(const InstalledWebappProvider&) = delete;
  InstalledWebappProvider& operator=(const InstalledWebappProvider&) = delete;

  ~InstalledWebappProvider() override;

  // ProviderInterface implementations.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito,
      const content_settings::PartitionKey& partition_key) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const content_settings::ContentSettingConstraints& constraints,
      const content_settings::PartitionKey& partition_key) override;

  void ClearAllContentSettingsRules(
      ContentSettingsType content_type,
      const content_settings::PartitionKey& partition_key) override;
  void ShutdownOnUIThread() override;

  void Notify(ContentSettingsType content_type);
};

#endif  // CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_
