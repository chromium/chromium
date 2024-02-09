// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for objects providing content setting rules.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PROVIDER_H_

#include <memory>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"

class ContentSettingsPattern;

namespace content_settings {

class RuleIterator;
struct Rule;

class ProviderInterface {
 public:
  virtual ~ProviderInterface() = default;

  // Returns a |RuleIterator| over the content setting rules stored by this
  // provider. If |off_the_record| is true, the iterator returns only the
  // content settings which are applicable to the incognito mode and differ from
  // the normal mode. Otherwise, it returns the content settings for the normal
  // mode. It is not allowed to call other |ProviderInterface| functions
  // (including |GetRuleIterator|) for the same provider until the
  // |RuleIterator| is destroyed.
  // Returns nullptr to indicate the RuleIterator is empty.
  //
  // This method needs to be thread-safe and continue to work after
  // |ShutdownOnUIThread| has been called.
  virtual std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool off_the_record,
      const PartitionKey& partition_key) const = 0;

  // Returns a ContentSettings Rule object if any rule stored by this provider
  // matched primary_url and secondary_url. This method allows for more
  // efficient lookups than GetRuleIterator.
  //
  // This method needs to be thread-safe and continue to work after
  // |ShutdownOnUIThread| has been called.
  virtual std::unique_ptr<Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record,
      const PartitionKey& partition_key) const;

  // Asks the provider to set the website setting for a particular
  // |primary_pattern|, |secondary_pattern|, |content_type| tuple. If the
  // provider accepts the setting it returns true and takes the ownership of the
  // |value|. Otherwise false is returned and the ownership of the |value| stays
  // with the caller.
  //
  // This should only be called on the UI thread, and not after
  // ShutdownOnUIThread has been called.
  virtual bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const ContentSettingConstraints& constraints,
      const PartitionKey& partition_key) = 0;

  // Resets all content settings for the given |content_type| and empty resource
  // identifier to CONTENT_SETTING_DEFAULT.
  //
  // This should only be called on the UI thread, and not after
  // ShutdownOnUIThread has been called.
  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type,
      const PartitionKey& partition_key) = 0;

  // Detaches the Provider from all Profile-related objects like PrefService.
  // This methods needs to be called before destroying the Profile.
  // Afterwards, none of the methods above that should only be called on the UI
  // thread should be called anymore.
  virtual void ShutdownOnUIThread() = 0;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PROVIDER_H_
