// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_HELPER_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_HELPER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_types.h"

namespace sessions {

class TabRestoreServiceImpl;
class TabRestoreServiceClient;
class LiveTabContext;
class TabRestoreServiceObserver;
class TimeFactory;

// Helper class used to implement TabRestoreService. See tab_restore_service.h
// for method-level comments.
class SESSIONS_EXPORT TabRestoreServiceHelper
    : public base::trace_event::MemoryDumpProvider {
 public:
  typedef TabRestoreService::DeletionPredicate DeletionPredicate;
  typedef TabRestoreService::Entries Entries;
  typedef tab_restore::Entry Entry;
  typedef tab_restore::Tab Tab;
  typedef tab_restore::TimeFactory TimeFactory;
  typedef tab_restore::Window Window;
  typedef tab_restore::Group Group;

  // Provides a way for the client to add behavior to the tab restore service
  // helper (e.g. implementing tabs persistence).
  class Observer {
   public:
    // Invoked before the entries are cleared.
    virtual void OnClearEntries();

    // Invoked when navigations from entries have been deleted.
    virtual void OnNavigationEntriesDeleted();

    // Invoked before the entry is restored. |entry_iterator| points to the
    // entry corresponding to the session identified by |id|.
    virtual void OnRestoreEntryById(SessionID id,
                                    Entries::const_iterator entry_iterator);

    // Invoked after an entry was added.
    virtual void OnAddEntry();

   protected:
    virtual ~Observer();
  };

  enum {
  // Max number of entries we'll keep around.
#if BUILDFLAG(IS_ANDROID)
    // Android keeps at most 5 recent tabs.
    kMaxEntries = 5,
#else
    kMaxEntries = 25,
#endif
  };

  // Creates a new TabRestoreServiceHelper and provides an object that provides
  // the current time. The TabRestoreServiceHelper does not take ownership of
  // |time_factory| and |observer|. Note that |observer| can also be NULL.
  TabRestoreServiceHelper(TabRestoreService* tab_restore_service,
                          TabRestoreServiceClient* client,
                          TimeFactory* time_factory);

  TabRestoreServiceHelper(const TabRestoreServiceHelper&) = delete;
  TabRestoreServiceHelper& operator=(const TabRestoreServiceHelper&) = delete;

  ~TabRestoreServiceHelper() override;

  void SetHelperObserver(Observer* observer);

  // Helper methods used to implement TabRestoreService.
  void AddObserver(TabRestoreServiceObserver* observer);
  void RemoveObserver(TabRestoreServiceObserver* observer);
  std::optional<SessionID> CreateHistoricalTab(LiveTab* live_tab, int index);
  void BrowserClosing(LiveTabContext* context);
  void BrowserClosed(LiveTabContext* context);
  void CreateHistoricalGroup(LiveTabContext* context,
                             const tab_groups::TabGroupId& id);
  void GroupClosed(const tab_groups::TabGroupId& group);
  void GroupCloseStopped(const tab_groups::TabGroupId& group);
  void ClearEntries();
  void DeleteNavigationEntries(const DeletionPredicate& predicate);

  const Entries& entries() const;
  std::vector<LiveTab*> RestoreMostRecentEntry(LiveTabContext* context);
  void RemoveEntryById(SessionID id);
  std::vector<LiveTab*> RestoreEntryById(LiveTabContext* context,
                                         SessionID id,
                                         WindowOpenDisposition disposition);
  bool IsRestoring() const;

  // Notifies observers the entries have changed.
  void NotifyEntriesChanged();

  // Notifies observers the service has loaded.
  void NotifyLoaded();

  // Adds |entry| to the list of entries. If |prune| is true |PruneAndNotify| is
  // invoked. If |to_front| is true the entry is added to the front, otherwise
  // the back. Normal closes go to the front, but tab/window closes from the
  // previous session are added to the back.
  void AddEntry(std::unique_ptr<Entry> entry, bool prune, bool to_front);

  // Prunes |entries_| to contain only kMaxEntries, and removes uninteresting
  // entries.
  void PruneEntries();

  // Returns an iterator into |entries_| whose id or original_id matches |id|.
  // If |id| identifies a Window, then its iterator position will be returned.
  // If it identifies a tab, then the iterator position of the Window in which
  // the Tab resides is returned.
  Entries::iterator GetEntryIteratorById(SessionID id);

  // From base::trace_event::MemoryDumpProvider
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Calls ValidateTab, ValidateWindow, or ValidateGroup as appropriate.
  static bool ValidateEntry(const Entry& entry);

 private:
  friend class TabRestoreServiceImpl;

  // Populates the tab's navigations from the LiveTab, and its browser_id and
  // pinned state from the context.
  void PopulateTab(Tab* tab,
                   int index,
                   LiveTabContext* context,
                   LiveTab* live_tab);

  // This is a helper function for RestoreEntryById() for restoring a single
  // tab. If |context| is NULL, this creates a new window for the entry. This
  // returns the LiveTabContext into which the tab was restored. |disposition|
  // will be respected, but if it is UNKNOWN then the tab's original attributes
  // will be respected instead. If a new LiveTabContext needs to be created for
  // this tab, If present, |live_tab| will be populated with the LiveTab of the
  // restored tab.
  // |original_session_type| indicates the type of session entry the tab
  // belongs to.
  LiveTabContext* RestoreTab(const Tab& tab,
                             LiveTabContext* context,
                             WindowOpenDisposition disposition,
                             sessions::tab_restore::Type session_restore_type,
                             LiveTab** live_tab);

  // This is a helper function for RestoreEntryById(). Restores a single entry
  // from the `window`. The entry to restore is denoted by `id` and can either
  // be a single tab or an entire group.
  LiveTabContext* RestoreTabOrGroupFromWindow(Window& window,
                                              SessionID id,
                                              LiveTabContext* context,
                                              WindowOpenDisposition disposition,
                                              std::vector<LiveTab*>* live_tabs);

  // Helper function for CreateHistoricalGroup. Returns a Group populated with
  // metadta for the tab group `id`.
  std::unique_ptr<Group> CreateHistoricalGroupImpl(
      LiveTabContext* context,
      const tab_groups::TabGroupId& id);

  // Returns true if |tab| has at least one navigation and
  // |tab->current_navigation_index| is in bounds.
  static bool ValidateTab(const Tab& tab);

  // Validates all the tabs in a window, plus the window's active tab index.
  static bool ValidateWindow(const Window& window);

  // Validates all the tabs in a group.
  static bool ValidateGroup(const Group& group);

  // Removes all navigation entries matching |predicate| from |tab|.
  // Returns true if |tab| should be deleted because it is empty.
  static bool DeleteFromTab(const DeletionPredicate& predicate, Tab* tab);

  // Removes all navigation entries matching |predicate| from tabs in |window|.
  // Returns true if |window| should be deleted because it is empty.
  static bool DeleteFromWindow(const DeletionPredicate& predicate,
                               Window* window);

  // Removes all navigation entries matching |predicate| from tabs in |group|.
  // Returns true if |group| should be deleted because it is empty.
  static bool DeleteFromGroup(const DeletionPredicate& predicate, Group* group);

  // Returns true if |tab| is one we care about restoring.
  bool IsTabInteresting(const Tab& tab);

  // Checks whether |window| is interesting --- if it only contains a single,
  // uninteresting tab, it's not interesting.
  bool IsWindowInteresting(const Window& window);

  // Checks whether |group| is interesting -- as long as it contains tabs,
  // it is.
  bool IsGroupInteresting(const Group& group);

  // Validates and checks |entry| for interesting.
  bool FilterEntry(const Entry& entry);

  // Finds tab entries with the old browser_id and sets it to the new one.
  void UpdateTabBrowserIDs(SessionID::id_type old_id, SessionID new_id);

  // Gets the current time. This uses the time_factory_ if there is one.
  base::Time TimeNow() const;

  const raw_ptr<TabRestoreService> tab_restore_service_;

  raw_ptr<Observer> observer_;

  raw_ptr<TabRestoreServiceClient> client_;

  // Set of entries. They are ordered from most to least recent.
  Entries entries_;

  // Are we restoring a tab? If this is true we ignore requests to create a
  // historical tab.
  bool restoring_;

  base::ObserverList<TabRestoreServiceObserver>::Unchecked observer_list_;

  // Set of contexts that we've received a BrowserClosing method for but no
  // corresponding BrowserClosed. We cache the set of contexts closing to
  // avoid creating historical tabs for them.
  std::set<raw_ptr<LiveTabContext, SetExperimental>> closing_contexts_;

  // Set of groups that we've received a CreateHistoricalGroup method for but no
  // corresponding GroupClosed. We cache the set of groups closing to avoid
  // creating historical tabs for them.
  std::set<tab_groups::TabGroupId> closing_groups_;

  const raw_ptr<TimeFactory> time_factory_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_HELPER_H_
