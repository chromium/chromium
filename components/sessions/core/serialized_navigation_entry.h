// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_ENTRY_H_
#define COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_ENTRY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sessions/core/sessions_export.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace base {
class Pickle;
class PickleIterator;
}

namespace sessions {

class SerializedNavigationEntryTestHelper;

// SerializedNavigationEntry is a "freeze-dried" version of NavigationEntry.  It
// contains the data needed to restore a NavigationEntry during session restore
// and tab restore, and it can also be pickled and unpickled.
//
// Default copy constructor and assignment operator welcome.
class SESSIONS_EXPORT SerializedNavigationEntry {
 public:
  enum BlockedState {
    STATE_INVALID = 0,
    STATE_ALLOWED = 1,
    STATE_BLOCKED = 2,
  };

  // These must match the proto.  They are in priority order such that if a
  // higher value is seen, it should overwrite a lower value.
  enum PasswordState {
    PASSWORD_STATE_UNKNOWN = 0,
    NO_PASSWORD_FIELD = 1,
    HAS_PASSWORD_FIELD = 2,
  };

  // Creates an invalid (index < 0) SerializedNavigationEntry.
  SerializedNavigationEntry();
  SerializedNavigationEntry(const SerializedNavigationEntry& other);
  SerializedNavigationEntry(SerializedNavigationEntry&& other) noexcept;
  ~SerializedNavigationEntry();

  SerializedNavigationEntry& operator=(const SerializedNavigationEntry& other);
  SerializedNavigationEntry& operator=(SerializedNavigationEntry&& other);

  // Note that not all SerializedNavigationEntry fields are preserved.
  // |max_size| is the max number of bytes to write.
  void WriteToPickle(int max_size, base::Pickle* pickle) const;
  bool ReadFromPickle(base::PickleIterator* iterator);

  // The index in the NavigationController. This SerializedNavigationEntry is
  // valid only when the index is non-negative.
  int index() const { return index_; }
  void set_index(int index) { index_ = index; }

  int unique_id() const { return unique_id_; }
  void set_unique_id(int unique_id) { unique_id_ = unique_id; }
  const std::u16string& title() const { return title_; }
  void set_title(const std::u16string& title) { title_ = title; }
  const GURL& favicon_url() const { return favicon_url_; }
  void set_favicon_url(const GURL& favicon_url) { favicon_url_ = favicon_url; }
  int http_status_code() const { return http_status_code_; }
  void set_http_status_code(int http_status_code) {
    http_status_code_ = http_status_code;
  }
  ui::PageTransition transition_type() const {
    return transition_type_;
  }
  void set_transition_type(ui::PageTransition transition_type) {
    transition_type_ = transition_type;
  }
  bool has_post_data() const { return has_post_data_; }
  int64_t post_id() const { return post_id_; }
  bool is_overriding_user_agent() const { return is_overriding_user_agent_; }
  base::Time timestamp() const { return timestamp_; }
  void set_timestamp(base::Time timestamp) { timestamp_ = timestamp; }

  BlockedState blocked_state() const { return blocked_state_; }
  void set_blocked_state(BlockedState blocked_state) {
    blocked_state_ = blocked_state;
  }

  PasswordState password_state() const { return password_state_; }
  void set_password_state(PasswordState password_state) {
    password_state_ = password_state;
  }

  const GURL& virtual_url() const { return virtual_url_; }
  void set_virtual_url(const GURL& virtual_url) { virtual_url_ = virtual_url; }

  const std::string& encoded_page_state() const { return encoded_page_state_; }
  void set_encoded_page_state(const std::string& encoded_page_state) {
    encoded_page_state_ = encoded_page_state;
  }

  const GURL& original_request_url() const { return original_request_url_; }
  void set_original_request_url(const GURL& original_request_url) {
    original_request_url_ = original_request_url;
  }

  const GURL& referrer_url() const { return referrer_url_; }
  void set_referrer_url(const GURL& referrer_url) {
    referrer_url_ = referrer_url;
  }

  int referrer_policy() const { return referrer_policy_; }
  void set_referrer_policy(int referrer_policy) {
    referrer_policy_ = referrer_policy;
  }

  const std::vector<GURL>& redirect_chain() const { return redirect_chain_; }

  bool is_restored() const { return is_restored_; }
  void set_is_restored(bool is_restored) { is_restored_ = is_restored; }

  const std::map<std::string, std::string>& extended_info_map() const {
    return extended_info_map_;
  }

  int64_t task_id() const { return task_id_; }
  void set_task_id(int64_t task_id) { task_id_ = task_id; }

  int64_t parent_task_id() const { return parent_task_id_; }
  void set_parent_task_id(int64_t parent_task_id) {
    parent_task_id_ = parent_task_id;
  }

  int64_t root_task_id() const { return root_task_id_; }
  void set_root_task_id(int64_t root_task_id) { root_task_id_ = root_task_id; }

  size_t EstimateMemoryUsage() const;

 private:
  friend class ContentSerializedNavigationBuilder;
  friend class SerializedNavigationEntryTestHelper;
  friend class IOSSerializedNavigationBuilder;
  friend class IOSSerializedNavigationDriver;

  // Index in the NavigationController.
  int index_ = -1;

  // Member variables corresponding to NavigationEntry fields.
  // If you add a new field that can allocate memory, please also add
  // it to the EstimatedMemoryUsage() implementation.
  int unique_id_ = 0;
  GURL referrer_url_;
  int referrer_policy_;
  GURL virtual_url_;
  std::u16string title_;
  std::string encoded_page_state_;
  ui::PageTransition transition_type_ = ui::PAGE_TRANSITION_TYPED;
  bool has_post_data_ = false;
  int64_t post_id_ = -1;
  GURL original_request_url_;
  bool is_overriding_user_agent_ = false;
  base::Time timestamp_;
  GURL favicon_url_;
  int http_status_code_ = 0;
  bool is_restored_ = false;          // Not persisted.
  std::vector<GURL> redirect_chain_;  // Not persisted.

  // Additional information.
  BlockedState blocked_state_ = STATE_INVALID;
  PasswordState password_state_ = PASSWORD_STATE_UNKNOWN;

  // Provides storage for arbitrary key/value pairs used by features. This
  // data is not synced.
  std::map<std::string, std::string> extended_info_map_;

  // These fields are stored in the 'SupportsUserData' fields of a
  // NavigationEntry (see SetUserData() and GetUserData() in navigation_entry.h
  int64_t task_id_ = -1;
  int64_t parent_task_id_ = -1;
  int64_t root_task_id_ = -1;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_ENTRY_H_
