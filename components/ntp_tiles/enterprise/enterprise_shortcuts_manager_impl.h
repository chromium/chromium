// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_MANAGER_IMPL_H_
#define COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_MANAGER_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcut.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_manager.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_store.h"

class PrefService;

namespace ntp_tiles {

class EnterpriseShortcutsManagerImpl : public EnterpriseShortcutsManager {
 public:
  explicit EnterpriseShortcutsManagerImpl(PrefService* prefs);

  EnterpriseShortcutsManagerImpl(const EnterpriseShortcutsManagerImpl&) =
      delete;
  EnterpriseShortcutsManagerImpl& operator=(
      const EnterpriseShortcutsManagerImpl&) = delete;

  ~EnterpriseShortcutsManagerImpl() override;

  // EnterpriseShortcutsManager implementation.
  void RestorePolicyLinks() override;

  const std::vector<EnterpriseShortcut>& GetLinks() const override;

  bool UpdateLink(const GURL& url, const std::u16string& title) override;
  bool ReorderLink(const GURL& url, size_t new_pos) override;
  bool DeleteLink(const GURL& url) override;
  bool UndoAction() override;

  base::CallbackListSubscription RegisterCallbackForOnChanged(
      base::RepeatingClosure callback) override;

  // Register preferences used by this class.
  static void RegisterProfilePrefs(PrefRegistrySimple* user_prefs);

 private:
  // Clears all links from |previous_links_|, |current_links_|, and user links
  // from the store.
  void ClearLinks();

  // Stores the current list to the profile's preferences. Does not notify
  // |OnStoreLinksChanged|.
  void StoreLinks();

  // Called when the links in the store have been modified from outside sources.
  // Saves the new set of links in |current_links_| and notifies
  // |closure_list_|.
  void OnStoreLinksChanged();

  const raw_ptr<PrefService> prefs_;
  EnterpriseShortcutsStore store_;
  std::vector<EnterpriseShortcut> current_links_;
  // The state of the current list of links before the last action was
  // performed.
  std::optional<std::vector<EnterpriseShortcut>> previous_links_;

  // List of closures to be invoked when custom links are updated by outside
  // sources.
  base::RepeatingClosureList closure_list_;

  // Observer for link changes by the store.
  base::CallbackListSubscription store_subscription_;

  base::WeakPtrFactory<EnterpriseShortcutsManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_MANAGER_IMPL_H_
