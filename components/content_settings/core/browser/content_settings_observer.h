// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OBSERVER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OBSERVER_H_

#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

class PartitionKey;

class Observer {
 public:
  // partition_key is nullptr if the change is not specific to a particular
  // PartitionKey.
  //
  // TODO(b/307193732): migrate the implementors of the other methods and remove
  // them.
  virtual void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set,
      const PartitionKey* partition_key) {}

  virtual void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) {}

  // Deprecated. Use the method above instead.
  // TODO(crbug.com/40196354): Migrate remaining clients and remove this method.
  virtual void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type) {}

 protected:
  virtual ~Observer() = default;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OBSERVER_H_
