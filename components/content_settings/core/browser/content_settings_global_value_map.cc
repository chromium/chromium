// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_global_value_map.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

namespace {

class RuleIteratorSimple : public RuleIterator {
 public:
  explicit RuleIteratorSimple(ContentSettingsType type,
                              PermissionSetting setting)
      : setting_(setting),
        info_(PermissionSettingsRegistry::GetInstance()->Get(type)) {}

  RuleIteratorSimple(const RuleIteratorSimple&) = delete;
  RuleIteratorSimple& operator=(const RuleIteratorSimple&) = delete;

  bool HasNext() const override { return !is_done_; }

  std::unique_ptr<Rule> Next() override {
    DCHECK(HasNext());
    is_done_ = true;
    return std::make_unique<Rule>(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        info_->delegate().ToValue(setting_), RuleMetaData{});
  }

 private:
  const PermissionSetting setting_;
  const raw_ptr<const PermissionSettingsInfo> info_;
  bool is_done_ = false;
};

}  // namespace

GlobalValueMap::GlobalValueMap() = default;

GlobalValueMap::~GlobalValueMap() = default;

std::unique_ptr<RuleIterator> GlobalValueMap::GetRuleIterator(
    ContentSettingsType content_type) const {
  auto it = settings_.find(content_type);
  if (it == settings_.end()) {
    return nullptr;
  }

  return std::make_unique<RuleIteratorSimple>(content_type, it->second);
}

void GlobalValueMap::SetPermissionSetting(
    ContentSettingsType content_type,
    std::optional<PermissionSetting> setting) {
  if (!setting) {
    settings_.erase(content_type);
  } else {
    settings_[content_type] = *setting;
  }
}

std::optional<PermissionSetting> GlobalValueMap::GetPermissionSetting(
    ContentSettingsType content_type) const {
  auto it = settings_.find(content_type);
  return it == settings_.end() ? std::nullopt : std::optional(it->second);
}

}  // namespace content_settings
