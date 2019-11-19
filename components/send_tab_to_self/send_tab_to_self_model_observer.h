// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_

#include <string>
#include <vector>

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Observer for the Send Tab To Self model. In the observer methods care should
// be taken to not modify the model.
class SendTabToSelfModelObserver {
 public:
  SendTabToSelfModelObserver() {}
  virtual ~SendTabToSelfModelObserver() {}

  // Invoked when the model has finished loading. Until this method is called it
  // is unsafe to use the model.
  // This call has overlaps with SendTabToSelfModel::IsReady but by having this
  // be a pure virtual function we can ensure that subclasses of this class will
  // have a way to ensure that the model is active before interacting with it.
  virtual void SendTabToSelfModelLoaded() = 0;

  // Invoked when elements of the model are added, removed, or updated. This is
  // the mechanism for the sync server to push changes in the state of the model
  // to clients.
  // TODO(crbug.com/945396) move EntriesAddedRemotely to use const refs to
  // clarify ownership.
  virtual void EntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>& new_entries) = 0;
  virtual void EntriesRemovedRemotely(
      const std::vector<std::string>& guids) = 0;
  // This observer will>>>>>>>> notify listeners of new and existing entries
  // that have been marked as opened.
  virtual void EntriesOpenedRemotely(
      const std::vector<const SendTabToSelfEntry*>& opened_entries) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfModelObserver);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_
