// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/firefox_importer.h"

#include <memory>
#include <set>
#include <string_view>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/importer/firefox_importer_utils.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_autofill_form_data_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_url_row.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/utility/importer/bookmark_html_reader.h"
#include "chrome/utility/importer/favicon_reencode.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_MAC)
#include "chrome/utility/importer/nss_decryptor.h"
#endif  // !BUILDFLAG(IS_MAC)

namespace {

// Original definition is in:
//   toolkit/components/places/nsINavBookmarksService.idl
enum BookmarkItemType {
  TYPE_BOOKMARK = 1,
  TYPE_FOLDER = 2,
  TYPE_SEPARATOR = 3,
  TYPE_DYNAMIC_CONTAINER = 4
};

// Loads the default bookmarks in the Firefox installed at |app_path|,
// and stores their locations in |urls|.
void LoadDefaultBookmarks(const base::FilePath& app_path,
                          std::set<GURL>* urls) {
  base::FilePath file = app_path.AppendASCII("defaults")
      .AppendASCII("profile")
      .AppendASCII("bookmarks.html");
  urls->clear();

  std::vector<ImportedBookmarkEntry> bookmarks;
  std::vector<importer::SearchEngineInfo> search_engines;
  bookmark_html_reader::ImportBookmarksFile(
      base::RepeatingCallback<bool(void)>(),
      base::RepeatingCallback<bool(const GURL&)>(), file, &bookmarks,
      &search_engines, nullptr);
  for (const auto& bookmark : bookmarks)
    urls->insert(bookmark.url);
}

// Returns true if |url| has a valid scheme that we allow to import. We
// filter out the URL with a unsupported scheme.
bool CanImportURL(const GURL& url) {
  // The URL is not valid.
  if (!url.is_valid())
    return false;

  // Filter out the URLs with unsupported schemes.
  const char* const kInvalidSchemes[] = {"wyciwyg", "place", "about", "chrome"};
  for (const auto* scheme : kInvalidSchemes) {
    if (url.SchemeIs(scheme))
      return false;
  }

  return true;
}

// Initializes |favicon_url| and |png_data| members of given FaviconUsageData
// structure with provided favicon data. Returns true if data is valid.
bool SetFaviconData(const std::string& icon_url,
                    const std::vector<unsigned char>& icon_data,
                    favicon_base::FaviconUsageData* usage_data) {
  usage_data->favicon_url = GURL(icon_url);

  // Don't bother importing favicons with invalid URLs.
  if (!usage_data->favicon_url.is_valid())
    return false;

  // Data must be valid.
  return !icon_data.empty() &&
         importer::ReencodeFavicon(&icon_data[0], icon_data.size(),
                                   &usage_data->png_data);
}

}  // namespace

struct FirefoxImporter::BookmarkItem {
  int parent;
  int id;
  GURL url;
  std::u16string title;
  BookmarkItemType type;
  std::string keyword;
  base::Time date_added;
  int64_t favicon;
  bool empty_folder;
};

FirefoxImporter::FirefoxImporter() = default;

FirefoxImporter::~FirefoxImporter() = default;

void FirefoxImporter::StartImport(const importer::SourceProfile& source_profile,
                                  uint16_t items,
                                  ImporterBridge* bridge) {
  bridge_ = bridge;
  source_path_ = source_profile.source_path;
  app_path_ = source_profile.app_path;

#if BUILDFLAG(IS_POSIX)
  locale_ = source_profile.locale;
#endif

  // The order here is important!
  bridge_->NotifyStarted();
  if (!source_path_copy_.CreateUniqueTempDir()) {
    bridge->NotifyEnded();
    return;
  }
  if ((items & importer::HOME_PAGE) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::HOME_PAGE);
    ImportHomepage();  // Doesn't have a UI item.
    bridge_->NotifyItemEnded(importer::HOME_PAGE);
  }

  // Note history should be imported before bookmarks because bookmark import
  // will also import favicons and we store favicon for a URL only if the URL
  // exist in history or bookmarks.
  if ((items & importer::HISTORY) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::HISTORY);
    ImportHistory();
    bridge_->NotifyItemEnded(importer::HISTORY);
  }

  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportBookmarks();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
#if !BUILDFLAG(IS_MAC)
  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    ImportPasswords();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }
#endif  // !BUILDFLAG(IS_MAC)
  if ((items & importer::AUTOFILL_FORM_DATA) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::AUTOFILL_FORM_DATA);
    ImportAutofillFormData();
    bridge_->NotifyItemEnded(importer::AUTOFILL_FORM_DATA);
  }
  bridge_->NotifyEnded();
}

void FirefoxImporter::ImportHistory() {
  base::FilePath file = GetCopiedSourcePath("places.sqlite");
  if (!base::PathExists(file))
    return;

  sql::Database db;
  if (!db.Open(file))
    return;

  // |visit_type| represent the transition type of URLs (typed, click,
  // redirect, bookmark, etc.) We eliminate some URLs like sub-frames and
  // redirects, since we don't want them to appear in history.
  // Firefox transition types are defined in:
  //   toolkit/components/places/nsINavHistoryService.idl
  const char query[] =
      "SELECT h.url, h.title, h.visit_count, "
      "h.hidden, h.typed, v.visit_date "
      "FROM moz_places h JOIN moz_historyvisits v "
      "ON h.id = v.place_id "
      "WHERE v.visit_type <= 3";

  sql::Statement s(db.GetUniqueStatement(query));

  std::vector<ImporterURLRow> rows;
  while (s.Step() && !cancelled()) {
    GURL url(s.ColumnString(0));

    // Filter out unwanted URLs.
    if (!CanImportURL(url))
      continue;

    ImporterURLRow row(url);
    row.title = s.ColumnString16(1);
    row.visit_count = s.ColumnInt(2);
    row.hidden = s.ColumnInt(3) == 1;
    row.typed_count = s.ColumnInt(4);
    row.last_visit = base::Time::FromTimeT(s.ColumnInt64(5)/1000000);

    rows.push_back(row);
  }

  if (!rows.empty() && !cancelled())
    bridge_->SetHistoryItems(rows, importer::VISIT_SOURCE_FIREFOX_IMPORTED);
}

void FirefoxImporter::ImportBookmarks() {
  base::FilePath file = GetCopiedSourcePath("places.sqlite");
  if (!base::PathExists(file))
    return;

  sql::Database db;
  if (!db.Open(file))
    return;

  // |moz_favicons| table has been introduced in Firefox 55 and is not available
  // in older Firefox profiles.
  FaviconsLocation favicons_location =
      db.IsSQLValid("SELECT count(*) FROM moz_favicons")
          ? FaviconsLocation::kPlacesDatabase
          : FaviconsLocation::kFaviconsDatabase;

  // Get the bookmark folders that we are interested in.
  int toolbar_folder_id = LoadNodeIDByGUID(&db, "toolbar_____");
  int menu_folder_id = LoadNodeIDByGUID(&db, "menu________");
  int unsorted_folder_id = LoadNodeIDByGUID(&db, "unfiled_____");

  // Load livemark IDs.
  std::set<int> livemark_id;
  LoadLivemarkIDs(&db, &livemark_id);

  // Load the default bookmarks.
  std::set<GURL> default_urls;
  LoadDefaultBookmarks(app_path_, &default_urls);

  BookmarkList list;
  GetTopBookmarkFolder(&db, toolbar_folder_id, &list);
  GetTopBookmarkFolder(&db, menu_folder_id, &list);
  GetTopBookmarkFolder(&db, unsorted_folder_id, &list);
  size_t count = list.size();
  for (size_t i = 0; i < count; ++i)
    GetWholeBookmarkFolder(&db, &list, i, favicons_location, nullptr);

  std::vector<ImportedBookmarkEntry> bookmarks;
  std::vector<importer::SearchEngineInfo> search_engines;
  FaviconMap favicon_map;

  // TODO(crbug.com/40304654): We do not support POST based keywords yet.
  // We won't include them in the list.
  std::set<int> post_keyword_ids;
  const char query[] =
      "SELECT b.id FROM moz_bookmarks b "
      "INNER JOIN moz_items_annos ia ON ia.item_id = b.id "
      "INNER JOIN moz_anno_attributes aa ON ia.anno_attribute_id = aa.id "
      "WHERE aa.name = 'bookmarkProperties/POSTData'";
  sql::Statement s(db.GetUniqueStatement(query));

  if (!s.is_valid())
    return;

  while (s.Step() && !cancelled())
    post_keyword_ids.insert(s.ColumnInt(0));

  for (const auto& item : list) {
    // Folders are added implicitly on adding children, so we only explicitly
    // add empty folders.
    if (item->type != TYPE_BOOKMARK &&
        ((item->type != TYPE_FOLDER) || !item->empty_folder))
      continue;

    if (CanImportURL(item->url)) {
      // Skip the default bookmarks and unwanted URLs.
      if (default_urls.find(item->url) != default_urls.end() ||
          post_keyword_ids.find(item->id) != post_keyword_ids.end())
        continue;

      // Find the bookmark path by tracing their links to parent folders.
      std::vector<std::u16string> path;
      BookmarkItem* child = item.get();
      bool found_path = false;
      bool is_in_toolbar = false;
      while (child->parent >= 0) {
        BookmarkItem* parent = list[child->parent].get();
        if (livemark_id.find(parent->id) != livemark_id.end()) {
          // Don't import live bookmarks.
          break;
        }

        if (parent->id != menu_folder_id) {
          // To avoid excessive nesting, omit the name for the bookmarks menu
          // folder.
          path.insert(path.begin(), parent->title);
        }

        if (parent->id == toolbar_folder_id)
          is_in_toolbar = true;

        if (parent->id == toolbar_folder_id ||
            parent->id == menu_folder_id ||
            parent->id == unsorted_folder_id) {
          // We've reached a root node, hooray!
          found_path = true;
          break;
        }

        child = parent;
      }

      if (!found_path)
        continue;

      ImportedBookmarkEntry entry;
      entry.creation_time = item->date_added;
      entry.title = item->title;
      entry.url = item->url;
      entry.path = path;
      entry.in_toolbar = is_in_toolbar;
      entry.is_folder = item->type == TYPE_FOLDER;

      bookmarks.push_back(entry);
    }

    if (item->type == TYPE_BOOKMARK) {
      if (item->favicon)
        favicon_map[item->favicon].insert(item->url);

      // Import this bookmark as a search engine if it has a keyword and its URL
      // is usable as a search engine URL. (Even if the URL doesn't allow
      // substitution, importing as a "search engine" allows users to trigger
      // the bookmark by entering its keyword in the omnibox.)
      if (item->keyword.empty())
        continue;
      importer::SearchEngineInfo search_engine_info;
      std::string search_engine_url;
      if (item->url.is_valid())
        search_engine_info.url = base::UTF8ToUTF16(item->url.spec());
      else if (bookmark_html_reader::CanImportURLAsSearchEngine(
                   item->url,
                   &search_engine_url))
        search_engine_info.url = base::UTF8ToUTF16(search_engine_url);
      else
        continue;
      search_engine_info.keyword = base::UTF8ToUTF16(item->keyword);
      search_engine_info.display_name = item->title;
      search_engines.push_back(search_engine_info);
    }
  }

  // Write into profile.
  if (!bookmarks.empty() && !cancelled()) {
    const std::u16string& first_folder_name =
        bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP_FROM_FIREFOX);
    bridge_->AddBookmarks(bookmarks, first_folder_name);
  }
  if (!search_engines.empty() && !cancelled()) {
    bridge_->SetKeywords(search_engines, false);
  }

  if (!cancelled()) {
    favicon_base::FaviconUsageDataList favicons;
    if (favicons_location == FaviconsLocation::kFaviconsDatabase) {
      DCHECK(favicon_map.empty());
      LoadFavicons(bookmarks, &favicons);
    } else if (!favicon_map.empty()) {
      LoadFavicons(&db, favicon_map, &favicons);
    }
    if (!favicons.empty())
      bridge_->SetFavicons(favicons);
  }
}

#if !BUILDFLAG(IS_MAC)
void FirefoxImporter::ImportPasswords() {
  // Initializes NSS3.
  NSSDecryptor decryptor;
  if (!decryptor.Init(source_path_, source_path_) &&
      !decryptor.Init(app_path_, source_path_)) {
    return;
  }

  // Since Firefox 32, passwords are in logins.json.
  base::FilePath json_file = source_path_.AppendASCII("logins.json");
  if (!base::PathExists(json_file))
    return;

  std::vector<importer::ImportedPasswordForm> forms;
  decryptor.ReadAndParseLogins(json_file, &forms);

  if (!cancelled()) {
    for (const auto& form : forms) {
      if (!form.username_value.empty() || !form.password_value.empty() ||
          form.blocked_by_user) {
        bridge_->SetPasswordForm(form);
      }
    }
  }
}
#endif  // !BUILDFLAG(IS_MAC)

void FirefoxImporter::ImportHomepage() {
  GURL home_page = GetHomepage(source_path_);
  if (home_page.is_valid() && !IsDefaultHomepage(home_page, app_path_)) {
    bridge_->AddHomePage(home_page);
  }
}

void FirefoxImporter::ImportAutofillFormData() {
  base::FilePath file = GetCopiedSourcePath("formhistory.sqlite");
  if (!base::PathExists(file))
    return;

  sql::Database db;
  if (!db.Open(file))
    return;

  const char query[] =
      "SELECT fieldname, value, timesUsed, firstUsed, lastUsed FROM "
      "moz_formhistory";

  sql::Statement s(db.GetUniqueStatement(query));

  std::vector<ImporterAutofillFormDataEntry> form_entries;
  while (s.Step() && !cancelled()) {
    ImporterAutofillFormDataEntry form_entry;
    form_entry.name = s.ColumnString16(0);
    form_entry.value = s.ColumnString16(1);
    form_entry.times_used = s.ColumnInt(2);
    form_entry.first_used = base::Time::FromTimeT(s.ColumnInt64(3) / 1000000);
    form_entry.last_used = base::Time::FromTimeT(s.ColumnInt64(4) / 1000000);

    // Don't import search bar history.
    if (base::UTF16ToUTF8(form_entry.name) == "searchbar-history")
      continue;

    form_entries.push_back(form_entry);
  }

  if (!form_entries.empty() && !cancelled())
    bridge_->SetAutofillFormData(form_entries);
}

int FirefoxImporter::LoadNodeIDByGUID(sql::Database* db,
                                      const std::string& GUID) {
  const char query[] =
      "SELECT id "
      "FROM moz_bookmarks "
      "WHERE guid == ?";
  sql::Statement s(db->GetUniqueStatement(query));
  s.BindString(0, GUID);

  if (!s.Step())
    return -1;
  return s.ColumnInt(0);
}

void FirefoxImporter::LoadLivemarkIDs(sql::Database* db,
                                      std::set<int>* livemark) {
  static const char kFeedAnnotation[] = "livemark/feedURI";
  livemark->clear();

  const char query[] =
      "SELECT b.item_id "
      "FROM moz_anno_attributes a "
      "JOIN moz_items_annos b ON a.id = b.anno_attribute_id "
      "WHERE a.name = ? ";
  sql::Statement s(db->GetUniqueStatement(query));
  s.BindString(0, kFeedAnnotation);

  while (s.Step() && !cancelled())
    livemark->insert(s.ColumnInt(0));
}

void FirefoxImporter::GetTopBookmarkFolder(sql::Database* db,
                                           int folder_id,
                                           BookmarkList* list) {
  const char query[] =
      "SELECT b.title "
      "FROM moz_bookmarks b "
      "WHERE b.type = 2 AND b.id = ? "
      "ORDER BY b.position";
  sql::Statement s(db->GetUniqueStatement(query));
  s.BindInt(0, folder_id);

  if (s.Step()) {
    std::unique_ptr<BookmarkItem> item = std::make_unique<BookmarkItem>();
    item->parent = -1;  // The top level folder has no parent.
    item->id = folder_id;
    item->title = s.ColumnString16(0);
    item->type = TYPE_FOLDER;
    item->favicon = 0;
    item->empty_folder = true;
    list->push_back(std::move(item));
  }
}

void FirefoxImporter::GetWholeBookmarkFolder(sql::Database* db,
                                             BookmarkList* list,
                                             size_t position,
                                             FaviconsLocation favicons_location,
                                             bool* empty_folder) {
  if (position >= list->size()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::string query =
      "SELECT b.id, h.url, COALESCE(b.title, h.title), "
      "b.type, k.keyword, b.dateAdded ";
  if (favicons_location == FaviconsLocation::kPlacesDatabase)
    query += ", h.favicon_id ";
  query +=
      "FROM moz_bookmarks b "
      "LEFT JOIN moz_places h ON b.fk = h.id "
      "LEFT JOIN moz_keywords k ON k.id = b.keyword_id "
      "WHERE b.type IN (1,2) AND b.parent = ? "
      "ORDER BY b.position";
  sql::Statement s(db->GetUniqueStatement(query));
  s.BindInt(0, (*list)[position]->id);

  BookmarkList temp_list;
  while (s.Step()) {
    std::unique_ptr<BookmarkItem> item = std::make_unique<BookmarkItem>();
    item->parent = static_cast<int>(position);
    item->id = s.ColumnInt(0);
    item->url = GURL(s.ColumnString(1));
    item->title = s.ColumnString16(2);
    item->type = static_cast<BookmarkItemType>(s.ColumnInt(3));
    item->keyword = s.ColumnString(4);
    item->date_added = base::Time::FromTimeT(s.ColumnInt64(5)/1000000);
    item->favicon = favicons_location == FaviconsLocation::kPlacesDatabase
                        ? s.ColumnInt64(6)
                        : 0;
    item->empty_folder = true;

    temp_list.push_back(std::move(item));
    if (empty_folder)
      *empty_folder = false;
  }

  // Appends all items to the list.
  for (auto& bookmark : temp_list) {
    list->push_back(std::move(bookmark));
    // Recursive add bookmarks in sub-folders.
    if (list->back()->type == TYPE_FOLDER) {
      GetWholeBookmarkFolder(db, list, list->size() - 1, favicons_location,
                             &list->back()->empty_folder);
    }
  }
}

void FirefoxImporter::LoadFavicons(
    sql::Database* db,
    const FaviconMap& favicon_map,
    favicon_base::FaviconUsageDataList* favicons) {
  const char query[] = "SELECT url, data FROM moz_favicons WHERE id=?";
  sql::Statement s(db->GetUniqueStatement(query));

  if (!s.is_valid())
    return;

  for (const auto& i : favicon_map) {
    s.BindInt64(0, i.first);
    if (s.Step()) {
      std::vector<unsigned char> data;
      if (!s.ColumnBlobAsVector(1, &data))
        continue;

      favicon_base::FaviconUsageData usage_data;
      if (!SetFaviconData(s.ColumnString(0), data, &usage_data))
        continue;

      usage_data.urls = i.second;
      favicons->push_back(usage_data);
    }
    s.Reset(true);
  }
}

void FirefoxImporter::LoadFavicons(
    const std::vector<ImportedBookmarkEntry>& bookmarks,
    favicon_base::FaviconUsageDataList* favicons) {
  base::FilePath file = GetCopiedSourcePath("favicons.sqlite");
  if (!base::PathExists(file))
    return;

  sql::Database db;
  if (!db.Open(file))
    return;

  sql::Statement s(db.GetUniqueStatement(
      "SELECT moz_icons.id, moz_icons.icon_url, moz_icons.data "
      "FROM moz_icons "
      "INNER JOIN moz_icons_to_pages "
      "ON moz_icons.id = moz_icons_to_pages.icon_id "
      "INNER JOIN moz_pages_w_icons "
      "ON moz_pages_w_icons.id = moz_icons_to_pages.page_id "
      "WHERE moz_pages_w_icons.page_url = ?"));
  if (!s.is_valid())
    return;

  // A map from icon id to the corresponding index in the |favicons| vector.
  std::map<uint64_t, size_t> icon_cache;

  for (const auto& entry : bookmarks) {
    // Reset the SQL statement at the start of the loop rather than at the end
    // to simplify early-continue logic.
    s.Reset(true);
    s.BindString(0, entry.url.spec());
    if (s.Step()) {
      uint64_t icon_id = s.ColumnInt64(0);
      auto it = icon_cache.find(icon_id);
      if (it != icon_cache.end()) {
        // A favicon that's used for multiple URLs. Append this URL to the list.
        (*favicons)[it->second].urls.insert(entry.url);
        continue;
      }

      std::vector<unsigned char> data;
      if (!s.ColumnBlobAsVector(2, &data))
        continue;

      favicon_base::FaviconUsageData usage_data;
      if (!SetFaviconData(s.ColumnString(1), data, &usage_data))
        continue;

      usage_data.urls.insert(entry.url);
      favicons->push_back(usage_data);
      icon_cache[icon_id] = favicons->size() - 1;
    }
  }
}

base::FilePath FirefoxImporter::GetCopiedSourcePath(
    std::string_view base_file_name) {
  const base::FilePath file = source_path_.AppendASCII(base_file_name);
  if (!base::PathExists(file))
    return {};
  // Temporary directory must be initialized.
  DCHECK(source_path_copy_.IsValid());
  const base::FilePath destination =
      source_path_copy_.GetPath().AppendASCII(base_file_name);
  if (!base::CopyFile(file, destination))
    return {};
  return destination;
}
