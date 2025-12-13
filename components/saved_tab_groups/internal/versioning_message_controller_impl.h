// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_VERSIONING_MESSAGE_CONTROLLER_IMPL_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_VERSIONING_MESSAGE_CONTROLLER_IMPL_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"

namespace tab_groups {

class TabGroupSyncService;

class VersioningMessageControllerImpl : public VersioningMessageController,
                                        public TabGroupSyncService::Observer {
 public:
  VersioningMessageControllerImpl(PrefService* pref_service,
                                  TabGroupSyncService* tab_group_sync_service);
  ~VersioningMessageControllerImpl() override;

  // VersioningMessageController implementation.
  bool IsInitialized() override;
  bool ShouldShowMessageUi(MessageType message_type) override;
  void ShouldShowMessageUiAsync(
      MessageType message_type,
      base::OnceCallback<void(bool)> callback) override;
  void OnMessageUiShown(MessageType message_type) override;
  void OnMessageUiDismissed(MessageType message_type) override;

  // TabGroupSyncService::Observer implementation.
  void OnInitialized() override;
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;

 private:
  void ComputePrefsOnStartup();
  void MaybeResolvePendingVersionUpdatedCallbacks();

  raw_ptr<PrefService> pref_service_;
  raw_ptr<TabGroupSyncService> tab_group_sync_service_;

  // Callbacks that are waiting for initialization to complete.
  std::vector<base::OnceClosure> pending_callbacks_;

  // Whether the TabGroupSyncService has been initialized.
  bool is_initialized_ = false;

  // Callbacks for VERSION_UPDATED_MESSAGE waiting for a final state
  // determination.
  std::vector<base::OnceCallback<void(bool)>>
      pending_version_updated_callbacks_;
  bool processed_version_updated_callbacks_ = false;

  base::WeakPtrFactory<VersioningMessageControllerImpl> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_VERSIONING_MESSAGE_CONTROLLER_IMPL_H_
