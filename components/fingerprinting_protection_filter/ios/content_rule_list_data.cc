// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/ios/content_rule_list_data.h"

namespace script_blocking {

// static
ContentRuleListData& ContentRuleListData::GetInstance() {
  static base::NoDestructor<ContentRuleListData> instance;
  return *instance;
}

ContentRuleListData::ContentRuleListData() = default;

ContentRuleListData::~ContentRuleListData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ContentRuleListData::SetContentRuleList(std::string content_rule_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content_rule_list_ = std::move(content_rule_list);
  // Notify all registered observers of the update.
  for (auto& observer : observers_) {
    observer.OnScriptBlockingRuleListUpdated(*content_rule_list_);
  }
}

const std::optional<std::string>& ContentRuleListData::GetContentRuleList()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_rule_list_;
}

void ContentRuleListData::AddObserver(ContentRuleListData::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);

  // If a rule list is already available, notify the new observer immediately
  // so it doesn't have to wait for the next update.
  if (content_rule_list_.has_value()) {
    observer->OnScriptBlockingRuleListUpdated(*content_rule_list_);
  }
}

void ContentRuleListData::RemoveObserver(
    ContentRuleListData::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void ContentRuleListData::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content_rule_list_.reset();
  // Observers should unregister themselves on their destruction at the end of
  // each test.
  CHECK(observers_.empty());
}

}  // namespace script_blocking
