// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/notice_card_tracker.h"

#include "base/time/time.h"
#include "base/values.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"

namespace feed {

namespace {
const char kViewsKey[] = "views";
const char kClicksKey[] = "clicks";
const char kDismissedKey[] = "dismissed";
}  // namespace

const int NoticeCardTracker::kViewCountThreshold = 3;
const int NoticeCardTracker::kClickCountThreshold = 1;

NoticeCardTracker::NoticeCardTracker(PrefService* profile_prefs,
                                     const std::string& key)
    : profile_prefs_(profile_prefs), key_(key) {
  DCHECK(profile_prefs_);
}

NoticeCardTracker::~NoticeCardTracker() = default;

// static
std::vector<std::string> NoticeCardTracker::GetAllAckowledgedKeys(
    PrefService* profile_prefs) {
  std::vector<std::string> keys;
  const base::Value* dict = profile_prefs->Get(prefs::kNoticeStates);
  if (dict && dict->is_dict()) {
    for (const auto pair : dict->DictItems()) {
      const auto& value = pair.second;
      if (value.is_dict()) {
        int views_count = 0;
        int clicks_count = 0;
        int dismissed = 0;
        for (const auto state_pair : value.DictItems()) {
          if (!state_pair.second.is_int())
            continue;
          if (state_pair.first == kViewsKey)
            views_count = state_pair.second.GetInt();
          else if (state_pair.first == kClicksKey)
            clicks_count = state_pair.second.GetInt();
          else if (state_pair.first == kDismissedKey)
            dismissed = state_pair.second.GetInt();
        }
        if (CanBeTreatedAsAcknowledged(views_count, clicks_count, dismissed)) {
          keys.push_back(pair.first);
        }
      }
    }
  }
  return keys;
}

// static
bool NoticeCardTracker::CanBeTreatedAsAcknowledged(int views_count,
                                                   int clicks_count,
                                                   int dismissed) {
  return (views_count >= kViewCountThreshold) ||
         (clicks_count >= kClickCountThreshold) || (dismissed > 0);
}

void NoticeCardTracker::OnViewed() {
  auto now = base::TimeTicks::Now();
  if (now - last_view_time_ < GetFeedConfig().minimum_notice_view_interval)
    return;

  last_view_time_ = now;

  IncrementViewCount();
}

void NoticeCardTracker::OnOpenAction() {
  IncrementClickCount();
}

void NoticeCardTracker::OnDismissed() {
  SetCount(kDismissedKey, 1);
}

bool NoticeCardTracker::HasAcknowledged() const {
  return CanBeTreatedAsAcknowledged(GetViewCount(), GetClickCount(),
                                    GetDismissState());
}

int NoticeCardTracker::GetViewCount() const {
  return GetCount(kViewsKey);
}

int NoticeCardTracker::GetClickCount() const {
  return GetCount(kClicksKey);
}

int NoticeCardTracker::GetDismissState() const {
  return GetCount(kDismissedKey);
}

void NoticeCardTracker::IncrementViewCount() {
  SetCount(kViewsKey, GetViewCount() + 1);
}

void NoticeCardTracker::IncrementClickCount() {
  SetCount(kClicksKey, GetClickCount() + 1);
}

const base::Value* NoticeCardTracker::GetStates() const {
  DCHECK(!key_.empty());

  const base::Value* dict = profile_prefs_->Get(prefs::kNoticeStates);
  if (!dict || !dict->is_dict())
    return nullptr;

  return dict->FindDictKey(key_);
}

int NoticeCardTracker::GetCount(base::StringPiece dict_key) const {
  const base::Value* value = GetStates();
  if (!value)
    return 0;
  return value->FindIntKey(dict_key).value_or(0);
}

void NoticeCardTracker::SetCount(base::StringPiece dict_key, int new_count) {
  base::Value updated_notices(base::Value::Type::DICTIONARY);
  base::Value updated_states(base::Value::Type::DICTIONARY);

  const base::Value* notices = profile_prefs_->Get(prefs::kNoticeStates);
  if (notices && notices->is_dict()) {
    updated_notices = notices->Clone();
    const base::Value* states = notices->FindDictKey(key_);
    if (states)
      updated_states = states->Clone();
  }

  updated_states.SetIntKey(dict_key, new_count);
  updated_notices.SetKey(key_, std::move(updated_states));

  profile_prefs_->Set(prefs::kNoticeStates, updated_notices);
}

}  // namespace feed
