// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_COMMON_MANAGER_BASE_H_
#define COMPONENTS_TPCD_METADATA_COMMON_MANAGER_BASE_H_

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"

namespace tpcd::metadata::common {

class ManagerBase {
 public:
  ManagerBase();
  ~ManagerBase();

  ManagerBase(const ManagerBase&) = delete;
  ManagerBase& operator=(const ManagerBase&) = delete;

  [[nodiscard]] ContentSetting GetContentSetting(
      const content_settings::HostIndexedContentSettings& grants,
      const GURL& third_party_url,
      const GURL& first_party_url,
      content_settings::SettingInfo* out_info) const;

  ContentSettingsForOneType GetContentSettingForOneType(
      const content_settings::HostIndexedContentSettings& grants) const;
};

}  // namespace tpcd::metadata::common
#endif  // COMPONENTS_TPCD_METADATA_COMMON_MANAGER_BASE_H_
