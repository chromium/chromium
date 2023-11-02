// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_DATABASE_H_
#define COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_DATABASE_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

// This class manages the shortcut provider table within the SQLite database
// passed to the constructor. It expects the following schema:
//
// Note: The database stores time in seconds, UTC.
//
// omni_box_shortcuts
//   id                  Unique id of the entry (needed for the sync).
//   search_text         Text that shortcuts was searched with.
//   url                 The url of the shortcut.
//   contents            Contents of the original omni-box entry.
//   contents_matches    Comma separated matches of the |search_text| in
//                       |contents|, for example "0,0,5,3,9,0".
//   description         Description of the original omni-box entry.
//   description_matches Comma separated matches of the |search_text| in
//                       |description|.
//   last_access_time    Time the entry was accessed last, stored in seconds,
//                       UTC.
//   number_of_hits      Number of times that the entry has been selected.
class ShortcutsDatabase : public base::RefCountedThreadSafe<ShortcutsDatabase> {
 public:
  // The following struct encapsulates one previously selected omnibox shortcut.
  struct Shortcut {
    // The fields of an AutocompleteMatch that we preserve in a shortcut.
    struct MatchCore {
      MatchCore(const std::u16string& fill_into_edit,
                const GURL& destination_url,
                AutocompleteMatch::DocumentType document_type,
                const std::u16string& contents,
                const std::string& contents_class,
                const std::u16string& description,
                const std::string& description_class,
                ui::PageTransition transition,
                AutocompleteMatchType::Type type,
                const std::u16string& keyword);
      MatchCore(const MatchCore& other);
      ~MatchCore();

      std::u16string fill_into_edit;
      GURL destination_url;
      AutocompleteMatch::DocumentType document_type;
      std::u16string contents;
      // For both contents_class and description_class, we strip MATCH
      // classifications; the ShortcutsProvider will re-mark MATCH regions based
      // on the user's current typing.
      std::string contents_class;
      std::u16string description;
      std::string description_class;
      ui::PageTransition transition;
      AutocompleteMatchType::Type type;
      std::u16string keyword;
    };

    Shortcut(const std::string& id,
             const std::u16string& text,
             const MatchCore& match_core,
             const base::Time& last_access_time,
             int number_of_hits);
    // Required for STL, we don't use this directly.
    Shortcut();
    Shortcut(const Shortcut& other);
    ~Shortcut();

    std::string id;  // Unique guid for the shortcut.
    std::u16string text;  // The user's original input string.
    MatchCore match_core;
    base::Time last_access_time;  // Last time shortcut was selected.
    int number_of_hits;           // How many times shortcut was selected.
  };

  typedef std::vector<std::string> ShortcutIDs;
  typedef std::map<std::string, Shortcut> GuidToShortcutMap;

  explicit ShortcutsDatabase(const base::FilePath& database_path);

  ShortcutsDatabase(const ShortcutsDatabase&) = delete;
  ShortcutsDatabase& operator=(const ShortcutsDatabase&) = delete;

  bool Init();

  // Adds the ShortcutsProvider::Shortcut to the database.
  bool AddShortcut(const Shortcut& shortcut);

  // Updates timing and selection count for the ShortcutsProvider::Shortcut.
  bool UpdateShortcut(const Shortcut& shortcut);

  // Deletes the ShortcutsProvider::Shortcuts with these IDs.
  bool DeleteShortcutsWithIDs(const ShortcutIDs& shortcut_ids);

  // Deletes the ShortcutsProvider::Shortcuts with the url.
  bool DeleteShortcutsWithURL(const std::string& shortcut_url_spec);

  // Deletes all of the ShortcutsProvider::Shortcuts.
  bool DeleteAllShortcuts();

  // Loads all of the shortcuts.
  void LoadShortcuts(GuidToShortcutMap* shortcuts);

 private:
  friend class base::RefCountedThreadSafe<ShortcutsDatabase>;
  friend class ShortcutsDatabaseTest;
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, AddShortcut);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, UpdateShortcut);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, DeleteShortcutsWithIds);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, DeleteShortcutsWithURL);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, LoadShortcuts);

  virtual ~ShortcutsDatabase();

  // Ensures that the table is present.
  bool EnsureTable();

  // Migrates table from version |version| - 1 to |version|. |version| = -1
  // indicates there is no preexisting table; |DoMigration| will migrate to the
  // latest version, skipping iterative migrations.
  bool DoMigration(int version);

  // The sql database. Not valid until Init is called.
  sql::Database db_;
  base::FilePath database_path_;

  sql::MetaTable meta_table_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_DATABASE_H_
