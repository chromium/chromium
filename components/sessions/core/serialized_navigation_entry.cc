// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/serialized_navigation_entry.h"

#include <stddef.h>

#include <tuple>
#include <utility>

#include "base/pickle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sessions/core/serialized_navigation_driver.h"

namespace sessions {

// The previous referrer policy value corresponding to |Never|.
const int kObsoleteReferrerPolicyNever = 2;

SerializedNavigationEntry::SerializedNavigationEntry() {
  referrer_policy_ =
      SerializedNavigationDriver::Get()->GetDefaultReferrerPolicy();
}

SerializedNavigationEntry::SerializedNavigationEntry(
    const SerializedNavigationEntry& other) = default;

SerializedNavigationEntry::SerializedNavigationEntry(
    SerializedNavigationEntry&& other) noexcept {
  // VC 2015 can't handle "noexcept = default" constructors. We want the
  // noexcept to avoid copying in a vector, but don't want to copy everything,
  // by hand, so fall on the default-generated move operator=.
  operator=(std::move(other));
}

SerializedNavigationEntry::~SerializedNavigationEntry() = default;

SerializedNavigationEntry& SerializedNavigationEntry::operator=(
    const SerializedNavigationEntry& other) = default;

SerializedNavigationEntry& SerializedNavigationEntry::operator=(
    SerializedNavigationEntry&& other) = default;

namespace {

// Helper used by SerializedNavigationEntry::WriteToPickle(). It writes |str| to
// |pickle|, if and only if |str| fits within (|max_bytes| -
// |*bytes_written|).  |bytes_written| is incremented to reflect the
// data written.
//
// TODO(akalin): Unify this with the same function in
// base_session_service.cc.
void WriteStringToPickle(base::Pickle* pickle,
                         int* bytes_written,
                         int max_bytes,
                         const std::string& str) {
  int num_bytes = str.size() * sizeof(char);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle->WriteString(str);
  } else {
    pickle->WriteString(std::string());
  }
}

// std::u16string version of WriteStringToPickle.
//
// TODO(akalin): Unify this, too.
void WriteString16ToPickle(base::Pickle* pickle,
                           int* bytes_written,
                           int max_bytes,
                           const std::u16string& str) {
  int num_bytes = str.size() * sizeof(char16_t);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle->WriteString16(str);
  } else {
    pickle->WriteString16(std::u16string());
  }
}

// A mask used for arbitrary boolean values needed to represent a
// NavigationEntry. Currently only contains HAS_POST_DATA.
//
// NOTE(akalin): We may want to just serialize |has_post_data_|
// directly.  Other bools (|is_overriding_user_agent_|) haven't been
// added to this mask.
enum TypeMask {
  HAS_POST_DATA = 1
};

}  // namespace

// Pickle order:
//
// index_
// virtual_url_
// title_
// encoded_page_state_
// transition_type_
//
// Added on later:
//
// type_mask (has_post_data_)
// referrer_url_
// referrer_policy_ (broken, crbug.com/450589)
// original_request_url_
// is_overriding_user_agent_
// timestamp_
// search_terms_ (removed)
// http_status_code_
// referrer_policy_
// extended_info_map_

void SerializedNavigationEntry::WriteToPickle(int max_size,
                                              base::Pickle* pickle) const {
  pickle->WriteInt(index_);

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_size,
                      virtual_url_.spec());

  WriteString16ToPickle(pickle, &bytes_written, max_size, title_);

  const std::string encoded_page_state =
      SerializedNavigationDriver::Get()->GetSanitizedPageStateForPickle(this);
  WriteStringToPickle(pickle, &bytes_written, max_size, encoded_page_state);

  pickle->WriteInt(transition_type_);

  const int type_mask = has_post_data_ ? HAS_POST_DATA : 0;
  pickle->WriteInt(type_mask);

  WriteStringToPickle(pickle, &bytes_written, max_size, referrer_url_.spec());

  // This field was deprecated in m61, but we still write it to the pickle for
  // forwards compatibility.
  pickle->WriteInt(kObsoleteReferrerPolicyNever);

  // Save info required to override the user agent.
  WriteStringToPickle(
      pickle, &bytes_written, max_size,
      original_request_url_.is_valid() ?
      original_request_url_.spec() : std::string());
  pickle->WriteBool(is_overriding_user_agent_);
  pickle->WriteInt64(timestamp_.ToInternalValue());

  // The |search_terms_| field was removed. Write an empty string to keep
  // backwards compatibility.
  WriteString16ToPickle(pickle, &bytes_written, max_size, std::u16string());

  pickle->WriteInt(http_status_code_);

  pickle->WriteInt(referrer_policy_);

  pickle->WriteInt(extended_info_map_.size());
  for (const auto& entry : extended_info_map_) {
    WriteStringToPickle(pickle, &bytes_written, max_size, entry.first);
    WriteStringToPickle(pickle, &bytes_written, max_size, entry.second);
  }

  pickle->WriteInt64(task_id_);
  pickle->WriteInt64(parent_task_id_);
  pickle->WriteInt64(root_task_id_);

  // This was used for the number of child task ids, followed by an int64
  // for each child task id.
  pickle->WriteInt(0);
}

bool SerializedNavigationEntry::ReadFromPickle(base::PickleIterator* iterator) {
  *this = SerializedNavigationEntry();
  std::string virtual_url_spec;
  int transition_type_int = 0;
  if (!iterator->ReadInt(&index_) ||
      !iterator->ReadString(&virtual_url_spec) ||
      !iterator->ReadString16(&title_) ||
      !iterator->ReadString(&encoded_page_state_) ||
      !iterator->ReadInt(&transition_type_int))
    return false;

  virtual_url_ = GURL(virtual_url_spec);
  // Fall back to PAGE_TRANSITION_LINK in case the entry is either corrupted or
  // format incompatible due to version skew.
  transition_type_ = ui::IsValidPageTransitionType(transition_type_int)
                         ? ui::PageTransitionFromInt(transition_type_int)
                         : ui::PAGE_TRANSITION_LINK;

  // type_mask did not always exist in the written stream. As such, we
  // don't fail if it can't be read.
  int type_mask = 0;
  bool has_type_mask = iterator->ReadInt(&type_mask);

  if (has_type_mask) {
    has_post_data_ = type_mask & HAS_POST_DATA;
    // the "referrer" property was added after type_mask to the written
    // stream. As such, we don't fail if it can't be read.
    std::string referrer_spec;
    if (!iterator->ReadString(&referrer_spec))
      referrer_spec = std::string();
    referrer_url_ = GURL(referrer_spec);

    // Note: due to crbug.com/450589 the initial referrer policy is incorrect,
    // and ignored. A correct referrer policy is extracted later (see
    // |correct_referrer_policy| below).
    int ignored_referrer_policy;
    std::ignore = iterator->ReadInt(&ignored_referrer_policy);

    // If the original URL can't be found, leave it empty.
    std::string original_request_url_spec;
    if (!iterator->ReadString(&original_request_url_spec))
      original_request_url_spec = std::string();
    original_request_url_ = GURL(original_request_url_spec);

    // Default to not overriding the user agent if we don't have info.
    if (!iterator->ReadBool(&is_overriding_user_agent_))
      is_overriding_user_agent_ = false;

    int64_t timestamp_internal_value = 0;
    if (iterator->ReadInt64(&timestamp_internal_value)) {
      timestamp_ = base::Time::FromInternalValue(timestamp_internal_value);
    } else {
      timestamp_ = base::Time();
    }

    // The |search_terms_| field was removed, but it still exists in the binary
    // format to keep backwards compatibility. Just get rid of it.
    std::u16string search_terms;
    std::ignore = iterator->ReadString16(&search_terms);

    if (!iterator->ReadInt(&http_status_code_))
      http_status_code_ = 0;

    // Correct referrer policy (if present).
    int correct_referrer_policy;
    if (iterator->ReadInt(&correct_referrer_policy)) {
      referrer_policy_ = correct_referrer_policy;
    } else {
      encoded_page_state_ =
          SerializedNavigationDriver::Get()->StripReferrerFromPageState(
              encoded_page_state_);
    }

    int extended_info_map_size = 0;
    if (iterator->ReadInt(&extended_info_map_size) &&
        extended_info_map_size > 0) {
      for (int i = 0; i < extended_info_map_size; ++i) {
        std::string key;
        std::string value;
        if (iterator->ReadString(&key) && iterator->ReadString(&value))
          extended_info_map_[key] = value;
      }
    }

    // Task IDs did not always exist in the written stream. As a result, we
    // don't fail if they can't be read
    if (!iterator->ReadInt64(&task_id_))
      task_id_ = -1;

    if (!iterator->ReadInt64(&parent_task_id_))
      parent_task_id_ = -1;

    if (!iterator->ReadInt64(&root_task_id_))
      root_task_id_ = -1;

    // Child task ids are no longer used.
    int children_task_ids_size = 0;
    std::ignore = iterator->ReadInt(&children_task_ids_size);
  }

  SerializedNavigationDriver::Get()->Sanitize(this);

  is_restored_ = true;

  return true;
}

size_t SerializedNavigationEntry::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(referrer_url_) +
         EstimateMemoryUsage(virtual_url_) + EstimateMemoryUsage(title_) +
         EstimateMemoryUsage(encoded_page_state_) +
         EstimateMemoryUsage(original_request_url_) +
         EstimateMemoryUsage(favicon_url_) +
         EstimateMemoryUsage(redirect_chain_) +
         EstimateMemoryUsage(extended_info_map_);
}

}  // namespace sessions
