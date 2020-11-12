// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_global_value_map.h"

#include <memory>
#include <utility>

#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

namespace {

class RuleIteratorSimple : public RuleIterator {
 public:
  RuleIteratorSimple(ContentSetting setting) : setting_(setting) {}

  bool HasNext() const override { return !is_done_; }

  Rule Next() override {
    DCHECK(HasNext());
    is_done_ = true;
    return Rule(ContentSettingsPattern::Wildcard(),
                ContentSettingsPattern::Wildcard(), base::Value(setting_),
                base::Time(), SessionModel::Durable);
  }

 private:
  const ContentSetting setting_;
  bool is_done_ = false;

  DISALLOW_COPY_AND_ASSIGN(RuleIteratorSimple);
};

}  // namespace

GlobalValueMap::GlobalValueMap() {}

GlobalValueMap::~GlobalValueMap() {}

std::unique_ptr<RuleIterator> GlobalValueMap::GetRuleIterator(
    ContentSettingsType content_type) const {
  auto it = settings_.find(content_type);
  if (it == settings_.end())
    return nullptr;

  return std::make_unique<RuleIteratorSimple>(it->second);
}

void GlobalValueMap::SetContentSetting(ContentSettingsType content_type,
                                       ContentSetting setting) {
  if (setting == CONTENT_SETTING_DEFAULT)
    settings_.erase(content_type);
  else
    settings_[content_type] = setting;
}

ContentSetting GlobalValueMap::GetContentSetting(
    ContentSettingsType content_type) const {
  auto it = settings_.find(content_type);
  return it == settings_.end() ? CONTENT_SETTING_DEFAULT : it->second;
}

}  // namespace content_settings
