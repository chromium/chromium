// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace collaboration::messaging {

// The TabGroupChangeNotifier is a class that listens to changes from
// TabGroupSyncService and passes them on to its own observers as delta updates
// of the tab group and tabs.
class TabGroupChangeNotifierImpl : public TabGroupChangeNotifier {
 public:
  explicit TabGroupChangeNotifierImpl(
      tab_groups::TabGroupSyncService* tab_group_sync_service);
  ~TabGroupChangeNotifierImpl() override;

  // TabGroupChangeNotifier.
  void AddObserver(TabGroupChangeNotifier::Observer* observer) override;
  void RemoveObserver(TabGroupChangeNotifier::Observer* observer) override;
  void Initialize() override;
  bool IsInitialized() override;

 private:
  // TabGroupSyncService::Observer.
  void OnInitialized() override;

  // Internal methods that synchronously informs observers of changes. These
  // are all invoked through callbacks to ensure all observers are invoked
  // asynchronously.
  void NotifyTabGroupChangeNotifierInitialized() const;

  // Whether the service has already been initialized.
  bool is_initialized_ = false;

  // Whether we currently have an observer added to the TabGroupSyncService.
  bool has_tab_group_sync_service_observer_ = false;

  // The list of observers observing this particular class.
  base::ObserverList<TabGroupChangeNotifier::Observer> observers_;

  // The TabGroupSyncService that is the source of the updates.
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  base::WeakPtrFactory<TabGroupChangeNotifierImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_TAB_GROUP_CHANGE_NOTIFIER_IMPL_H_
