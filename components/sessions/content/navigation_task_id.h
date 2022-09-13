// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_NAVIGATION_TASK_ID_H_
#define COMPONENTS_SESSIONS_CONTENT_NAVIGATION_TASK_ID_H_

#include <stdint.h>

#include "base/supports_user_data.h"
#include "components/sessions/core/sessions_export.h"

namespace content {
class NavigationEntry;
}

namespace sessions {

// Stores Task ID data in a NavigationEntry. Task IDs track navigations and
// relationships between navigations
class SESSIONS_EXPORT NavigationTaskId : public base::SupportsUserData::Data {
 public:
  NavigationTaskId();
  NavigationTaskId(const NavigationTaskId& navigation_task_id);
  ~NavigationTaskId() override;

  static NavigationTaskId* Get(content::NavigationEntry* entry);

  int64_t id() const { return id_; }

  int64_t parent_id() const { return parent_id_; }

  int64_t root_id() const { return root_id_; }

  void set_id(int64_t id) { id_ = id; }

  void set_parent_id(int64_t parent_id) { parent_id_ = parent_id; }

  void set_root_id(int64_t root_id) { root_id_ = root_id; }

  // base::SupportsUserData::Data:
  std::unique_ptr<base::SupportsUserData::Data> Clone() override;

 private:
  // A Task is a collection of navigations.
  //
  // A Task ID is an identifier of a Task. It is a Unique ID upon the first
  // navigation - navigating via the back button will not create a new ID but
  // the ID upon the first navigation will be used.
  //
  // A Parent Task ID is the identifier for the previous task in a series of
  // navigations.
  //
  // A Root Task ID is the first Task ID in a collection of navigations. Root
  // Task IDs are tracked for task clustering in the event that an intermediate
  // Tab is closed. It is not possible to group the tasks via a tree traversal
  // in this situation.
  int64_t id_ = -1;
  int64_t parent_id_ = -1;
  int64_t root_id_ = -1;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_NAVIGATION_TASK_ID_H_
