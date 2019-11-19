// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_BACKEND_H_
#define COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_BACKEND_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/shortcuts_database.h"
#include "components/search_engines/search_terms_data.h"
#include "url/gurl.h"

class ShortcutsBackend;
class TemplateURLService;
struct TestShortcutData;

void PopulateShortcutsBackendWithTestData(
    scoped_refptr<ShortcutsBackend> backend,
    TestShortcutData* db,
    size_t db_size);

namespace history {
class ShortcutsDatabase;
}  // namespace history

// This class manages the shortcut provider backend - access to database on the
// db thread, etc.
class ShortcutsBackend : public RefcountedKeyedService,
                         public history::HistoryServiceObserver {
 public:
  typedef std::multimap<base::string16, const ShortcutsDatabase::Shortcut>
      ShortcutMap;

  // For unit testing, set |suppress_db| to true to prevent creation
  // of the database, in which case all operations are performed in memory only.
  ShortcutsBackend(TemplateURLService* template_url_service,
                   std::unique_ptr<SearchTermsData> search_terms_data,
                   history::HistoryService* history_service,
                   base::FilePath database_path,
                   bool suppress_db);

  // The interface is guaranteed to be called on the thread AddObserver()
  // was called.
  class ShortcutsBackendObserver {
   public:
    // Called after the database is loaded and Init() completed.
    virtual void OnShortcutsLoaded() = 0;
    // Called when shortcuts changed (added/updated/removed) in the database.
    virtual void OnShortcutsChanged() {}

   protected:
    virtual ~ShortcutsBackendObserver() {}
  };

  // Asynchronously initializes the ShortcutsBackend, it is safe to call
  // multiple times - only the first call will be processed.
  bool Init();

  // All of the public functions *must* be called on UI thread only!

  bool initialized() const { return current_state_ == INITIALIZED; }
  const ShortcutMap& shortcuts_map() const { return shortcuts_map_; }

  // Deletes the Shortcuts with the url.
  bool DeleteShortcutsWithURL(const GURL& shortcut_url);

  // Deletes the Shortcuts that begin with the url.
  bool DeleteShortcutsBeginningWithURL(const GURL& shortcut_url);

  void AddObserver(ShortcutsBackendObserver* obs);
  void RemoveObserver(ShortcutsBackendObserver* obs);

  // Looks for an existing shortcut to match.destination_url that starts with
  // |text|.  Updates that shortcut if found, otherwise adds a new shortcut.
  void AddOrUpdateShortcut(const base::string16& text,
                           const AutocompleteMatch& match);

 private:
  friend class base::RefCountedThreadSafe<ShortcutsBackend>;
  friend class ShortcutsBackendTest;
  friend void PopulateShortcutsBackendWithTestData(
      scoped_refptr<ShortcutsBackend> backend,
      TestShortcutData* db,
      size_t db_size);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsBackendTest, EntitySuggestionTest);

  enum CurrentState {
    NOT_INITIALIZED,  // Backend created but not initialized.
    INITIALIZING,     // Init() called, but not completed yet.
    INITIALIZED,      // Initialization completed, all accessors can be safely
                      // called.
  };

  typedef std::map<std::string, ShortcutMap::iterator> GuidMap;

  ~ShortcutsBackend() override;

  static ShortcutsDatabase::Shortcut::MatchCore MatchToMatchCore(
      const AutocompleteMatch& match,
      TemplateURLService* template_url_service,
      SearchTermsData* search_terms_data);

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Internal initialization of the back-end. Posted by Init() to the DB thread.
  // On completion posts InitCompleted() back to UI thread.
  void InitInternal();

  // Finishes initialization on UI thread, notifies all observers.
  void InitCompleted();

  // Adds the Shortcut to the database.
  bool AddShortcut(const ShortcutsDatabase::Shortcut& shortcut);

  // Updates timing and selection count for the Shortcut.
  bool UpdateShortcut(const ShortcutsDatabase::Shortcut& shortcut);

  // Deletes the Shortcuts with these IDs.
  bool DeleteShortcutsWithIDs(
      const ShortcutsDatabase::ShortcutIDs& shortcut_ids);

  // Deletes all shortcuts whose URLs begin with |url|.  If |exact_match| is
  // true, only shortcuts from exactly |url| are deleted.
  bool DeleteShortcutsWithURL(const GURL& url, bool exact_match);

  // Deletes all of the shortcuts.
  bool DeleteAllShortcuts();

  TemplateURLService* template_url_service_;
  std::unique_ptr<SearchTermsData> search_terms_data_;

  CurrentState current_state_;
  base::ObserverList<ShortcutsBackendObserver>::Unchecked observer_list_;
  scoped_refptr<ShortcutsDatabase> db_;

  // The |temp_shortcuts_map_| and |temp_guid_map_| used for temporary storage
  // between InitInternal() and InitComplete() to avoid doing a potentially huge
  // copy.
  std::unique_ptr<ShortcutMap> temp_shortcuts_map_;
  std::unique_ptr<GuidMap> temp_guid_map_;

  ShortcutMap shortcuts_map_;
  // This is a helper map for quick access to a shortcut by guid.
  GuidMap guid_map_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};

  scoped_refptr<base::SequencedTaskRunner> main_runner_;
  scoped_refptr<base::SequencedTaskRunner> db_runner_;

  // For some unit-test only.
  bool no_db_access_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutsBackend);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_BACKEND_H_
