// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_

#include <string>

#include "base/containers/span.h"
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
  virtual void OnEntriesAddedRemotely(
      base::span<const SendTabToSelfEntry* const> new_entries) {}
  // Invoked when a new entry is added on the local device.
  virtual void OnEntryAddedLocally(const SendTabToSelfEntry* entry) {}
  // Invoked when entries are removed from the model by the sync server.
  // `guids` contains the unique identifiers of the removed entries.
  virtual void OnEntriesRemovedRemotely(base::span<const std::string> guids) {}
  // Invoked when new and existing entries have been marked as opened by the
  // sync server.
  virtual void OnEntriesOpenedRemotely(
      base::span<const SendTabToSelfEntry* const> opened_entries) {}
  // Invoked when the model becomes operational and ready to sync (initial data
  // is loaded).
  virtual void OnModelReady() {}
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_
