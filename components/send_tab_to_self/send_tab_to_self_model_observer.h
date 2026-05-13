// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_

#include <string>
#include <vector>

#include "base/observer_list_types.h"

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Observer for the Send Tab To Self model. In the observer methods care should
// be taken to not modify the model.
class SendTabToSelfModelObserver : public base::CheckedObserver {
 public:
  SendTabToSelfModelObserver() = default;

  SendTabToSelfModelObserver(const SendTabToSelfModelObserver&) = delete;
  SendTabToSelfModelObserver& operator=(const SendTabToSelfModelObserver&) =
      delete;

  ~SendTabToSelfModelObserver() override = default;


  // Invoked when new entries are added to the model by the  sync server.
  // TODO(crbug.com/40619926) move OnEntriesAddedRemotely to use const refs to
  // clarify ownership.
  virtual void OnEntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>& new_entries) = 0;
  // Invoked when a new entry is added on the local device.
  virtual void OnEntryAddedLocally(const SendTabToSelfEntry* entry) {}
  // Invoked when entries are removed from the model by the sync server.
  // `guids` contains the unique identifiers of the removed entries.
  virtual void OnEntriesRemovedRemotely(
      const std::vector<std::string>& guids) = 0;
  // Invoked when new and existing entries have been marked as opened by the
  // sync server.
  virtual void OnEntriesOpenedRemotely(
      const std::vector<const SendTabToSelfEntry*>& opened_entries) {}
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_
