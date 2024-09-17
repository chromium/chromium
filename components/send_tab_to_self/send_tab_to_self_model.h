// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "url/gurl.h"

namespace send_tab_to_self {

struct TargetDeviceInfo;

// The send tab to self model contains a list of entries of shared urls.
// This object should only be accessed from one thread, which is usually the
// main thread.
class SendTabToSelfModel {
 public:
  SendTabToSelfModel();

  SendTabToSelfModel(const SendTabToSelfModel&) = delete;
  SendTabToSelfModel& operator=(const SendTabToSelfModel&) = delete;

  virtual ~SendTabToSelfModel();

  // Returns a vector of entry IDs in the model.
  virtual std::vector<std::string> GetAllGuids() const = 0;

  // Returns a specific entry. Returns null if the entry does not exist.
  virtual const SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const = 0;

  // Adds |url| at the top of the entries. The entry title will be a
  // trimmed copy of |title|. Allows clients to modify the state of the model
  // as driven by user behaviors.
  // Returns the entry if it was successfully added.
  virtual const SendTabToSelfEntry* AddEntry(
      const GURL& url,
      const std::string& title,
      const std::string& target_device_cache_guid) = 0;

  // Remove entry with |guid| from entries. Allows clients to modify the state
  // of the model as driven by user behaviors.
  virtual void DeleteEntry(const std::string& guid) = 0;

  // Dismiss entry with key |guid|. Allows clients to modify the state
  // of the model as driven by user behaviors.
  virtual void DismissEntry(const std::string& guid) = 0;

  // If an entry with `guid` exists, marks it as opened.
  // Otherwise, the guid is queued in-memory, and if an entry with
  // that guid later arrives from another device, it'll be immediately
  // marked as opened. This can be used for platforms where
  // the tab can be additionally received/displayed by layers other than
  // SendTabToSelfModel, to avoid showing the same notification twice.
  virtual void MarkEntryOpened(const std::string& guid) = 0;

  // Guarantee that the model is operational and syncing, i.e., the local
  // database is started and the initial data has been downloaded.
  // This call and SendTabToSelfModelObserver::SendTabToSelfModelLoaded overlap,
  // but this call allows non observers to infer if it is safe to interact with
  // the model without first becoming an observer and creating a new bridge.
  // This provides a more direct path for classes that would like to modify the
  // model, but don't need to observe changes in it.
  virtual bool IsReady() = 0;

  // Observer registration methods. The model will remove all observers upon
  // destruction automatically.
  void AddObserver(SendTabToSelfModelObserver* observer);
  void RemoveObserver(SendTabToSelfModelObserver* observer);

  // Returns true if the user has valid target device.
  virtual bool HasValidTargetDevice() = 0;

  // Returns a vector of information about possible target devices, ordered by
  // the last updated time stamp of the device with the most recently used
  // device listed first. This is a thin layer on top of DeviceInfoTracker.
  virtual std::vector<TargetDeviceInfo> GetTargetDeviceInfoSortedList() = 0;

 protected:
  // The observers.
  base::ObserverList<SendTabToSelfModelObserver>::Unchecked observers_;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H_
