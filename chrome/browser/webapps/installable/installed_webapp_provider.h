// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_
#define CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_

#include <memory>
#include <vector>

#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

// PartitionKey is ignored by this provider because the content settings should
// apply across partitions.
class InstalledWebappProvider : public content_settings::ObservableProvider {
 public:
  // Although not used in the interface of this class, RuleList is the type for
  // the underlying data that this Provider holds.
  using RuleList = std::vector<std::pair<GURL, PermissionSetting>>;

  InstalledWebappProvider();

  InstalledWebappProvider(const InstalledWebappProvider&) = delete;
  InstalledWebappProvider& operator=(const InstalledWebappProvider&) = delete;

  ~InstalledWebappProvider() override;

  // ProviderInterface implementations.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito) const override;

  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const base::Value& value,
      const content_settings::ContentSettingConstraints& constraints) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;
  void ShutdownOnUIThread() override;

  void Notify(ContentSettingsType content_type);

 private:
  void RefreshRulesForType(ContentSettingsType content_type);

  content_settings::OriginValueMap value_map_;
  bool is_refreshing_ = false;
};

#endif  // CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_
