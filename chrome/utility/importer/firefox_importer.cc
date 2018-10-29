// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/firefox_importer.h"

#include <memory>
#include <set>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/importer/firefox_importer_utils.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_autofill_form_data_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_url_row.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/utility/importer/bookmark_html_reader.h"
#include "chrome/utility/importer/favicon_reencode.h"
#include "chrome/utility/importer/nss_decryptor.h"
#include "components/autofill/core/common/password_form.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace {

// Original definition is in http://mxr.mozilla.org/firefox/source/toolkit/
//  components/places/public/nsINavBookmarksService.idl
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
  bookmark_html_reader::ImportBookmarksFile(base::Callback<bool(void)>(),
                                            base::Callback<bool(const GURL&)>(),
                                            file,
                                            &bookmarks,
                                            &search_engines,
                                            NULL);
  for (size_t i = 0; i < bookmarks.size(); ++i)
    urls->insert(bookmarks[i].url);
}

// Returns true if |url| has a valid scheme that we allow to import. We
// filter out the URL with a unsupported scheme.
bool CanImportURL(const GURL& url) {
  // The URL is not valid.
  if (!url.is_valid())
    return false;

  // Filter out the URLs with unsupported schemes.
  const char* const kInvalidSchemes[] = {"wyciwyg", "place", "about", "chrome"};
  for (size_t i = 0; i < arraysize(kInvalidSchemes); ++i) {
    if (url.SchemeIs(kInvalidSchemes[i]))
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
  base::string16 title;
  BookmarkItemType type;
  std::string keyword;
  base::Time date_added;
  int64_t favicon;
  bool empty_folder;
};

FirefoxImporter::FirefoxImporter() {
}

FirefoxImporter::~FirefoxImporter() {
}

void FirefoxImporter::StartImport(const importer::SourceProfile& source_profile,
                                  uint16_t items,
                                  ImporterBridge* bridge) {
  UMA_HISTOGRAM_BOOLEAN("Import.IncludesPasswords.Firefox",
                        !!(items & importer::PASSWORDS));

  bridge_ = bridge;
  source_path_ = source_profile.source_path;
  app_path_ = source_profile.app_path;

#if defined(OS_POSIX)
  locale_ = source_profile.locale;
#endif

  // The order here is important!
  bridge_->NotifyStarted();
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
  if ((items & importer::SEARCH_ENGINES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::SEARCH_ENGINES);
    ImportSearchEngines();
    bridge_->NotifyItemEnded(importer::SEARCH_ENGINES);
  }
  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    ImportPasswords();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }
  if ((items & importer::AUTOFILL_FORM_DATA) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::AUTOFILL_FORM_DATA);
    ImportAutofillFormData();
    bridge_->NotifyItemEnded(importer::AUTOFILL_FORM_DATA);
  }
  bridge_->NotifyEnded();
}

void FirefoxImporter::ImportHistory() {
  base::FilePath file = source_path_.AppendASCII("places.sqlite");
  if (!base::PathExists(file))
    return;

  sql::Database db;
  if (!db.Open(file))
    return;

  // |visit_type| represent the transition type of URLs (typed, click,
  // redirect, bookmark, etc.) We eliminate some URLs like sub-frames and
  // redirects, since we don't want them to appear in history.
  // Firefox transition types are defined in:
  //   toolkit/components/places/public/nsINavHistoryService.idl
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
  base::FilePath file = source_path_.AppendASCII("places.sqlite");
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

  // TODO(jcampan): http://b/issue?id=1196285 we do not support POST based
  //                keywords yet.  We won't include them in the list.
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
      std::vector<base::string16> path;
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
    const base::string16& first_folder_name =
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

void FirefoxImporter::ImportPasswords() {
  // Initializes NSS3.
  NSSDecryptor decryptor;
  if (!decryptor.Init(source_path_, source_path_) &&
      !decryptor.Init(app_path_, source_path_)) {
    return;
  }

  std::vector<autofill::PasswordForm> forms;
  base::FilePath source_path = source_path_;
  const base::FilePath sqlite_file = source_path.AppendASCII("signons.sqlite");
  const base::FilePath json_file = source_path.AppendASCII("logins.json");
  const base::FilePath signon3_file = source_path.AppendASCII("signons3.txt");
  const base::FilePath signon2_file = source_path.AppendASCII("signons2.txt");
  if (base::PathExists(json_file)) {
    // Since Firefox 32, passwords are in logins.json.
    decryptor.ReadAndParseLogins(json_file, &forms);
  } else if (base::PathExists(sqlite_file)) {
    // Since Firefox 3.1, passwords are in signons.sqlite db.
    decryptor.ReadAndParseSignons(sqlite_file, &forms);
  } else if (base::PathExists(signon3_file)) {
    // Firefox 3.0 uses signons3.txt to store the passwords.
    decryptor.ParseSignons(signon3_file, &forms);
  } else {
    decryptor.ParseSignons(signon2_file, &forms);
  }

  if (!cancelled()) {
    UMA_HISTOGRAM_COUNTS_10000("Import.NumberOfImportedPasswords.Firefox",
                               forms.size());
    for (size_t i = 0; i < forms.size(); ++i) {
      if (!forms[i].username_value.empty() ||
          !forms[i].password_value.empty() ||
          forms[i].blacklisted_by_user) {
        bridge_->SetPasswordForm(forms[i]);
      }
    }
  }
}

void FirefoxImporter::ImportSearchEngines() {
  std::vector<std::string> search_engine_data;
  GetSearchEnginesXMLData(&search_engine_data);

  bridge_->SetFirefoxSearchEnginesXMLData(search_engine_data);
}

void FirefoxImporter::ImportHomepage() {
  GURL home_page = GetHomepage(source_path_);
  if (home_page.is_valid() && !IsDefaultHomepage(home_page, app_path_)) {
    bridge_->AddHomePage(home_page);
  }
}

void FirefoxImporter::ImportAutofillFormData() {
  base::FilePath file = source_path_.AppendASCII("formhistory.sqlite");
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

void FirefoxImporter::GetSearchEnginesXMLData(
    std::vector<std::string>* search_engine_data) {
  base::FilePath file = source_path_.AppendASCII("search.sqlite");
  if (!base::PathExists(file)) {
    // Since Firefox 3.5, search engines are no longer stored in search.sqlite.
    // Instead, search.json is used for storing search engines.
    GetSearchEnginesXMLDataFromJSON(search_engine_data);
    return;
  }

  sql::Database db;
  if (!db.Open(file))
    return;

  const char query[] =
      "SELECT engineid FROM engine_data "
      "WHERE engineid NOT IN "
      "(SELECT engineid FROM engine_data "
      "WHERE name='hidden') "
      "ORDER BY value ASC";

  sql::Statement s(db.GetUniqueStatement(query));
  if (!s.is_valid())
    return;

  const base::FilePath searchplugins_path(FILE_PATH_LITERAL("searchplugins"));
  // Search engine definitions are XMLs stored in two directories. Default
  // engines are in the app directory (app_path_) and custom engines are
  // in the profile directory (source_path_).

  // Since Firefox 21, app_path_ engines are in 'browser' subdirectory:
  base::FilePath app_path =
      app_path_.AppendASCII("browser").Append(searchplugins_path);
  if (!base::PathExists(app_path)) {
    // This might be an older Firefox, try old location without the 'browser'
    // path component:
    app_path = app_path_.Append(searchplugins_path);
  }

  base::FilePath profile_path = source_path_.Append(searchplugins_path);

  // Firefox doesn't store a search engine in its sqlite database unless the
  // user has added a engine. So we get search engines from sqlite db as well
  // as from the file system.
  if (s.Step()) {
    const std::string kAppPrefix("[app]/");
    const std::string kProfilePrefix("[profile]/");
    do {
      base::FilePath file;
      std::string engine(s.ColumnString(0));

      // The string contains [app]/<name>.xml or [profile]/<name>.xml where
      // the [app] and [profile] need to be replaced with the actual app or
      // profile path.
      size_t index = engine.find(kAppPrefix);
      if (index != std::string::npos) {
        // Remove '[app]/'.
        file = app_path.AppendASCII(engine.substr(index + kAppPrefix.length()));
      } else if ((index = engine.find(kProfilePrefix)) != std::string::npos) {
        // Remove '[profile]/'.
          file = profile_path.AppendASCII(
              engine.substr(index + kProfilePrefix.length()));
      } else {
        // Looks like absolute path to the file.
        file = base::FilePath::FromUTF8Unsafe(engine);
      }
      std::string file_data;
      base::ReadFileToString(file, &file_data);
      search_engine_data->push_back(file_data);
    } while (s.Step() && !cancelled());
  }

#if defined(OS_POSIX)
  // Ubuntu-flavored Firefox supports locale-specific search engines via
  // locale-named subdirectories. They fall back to en-US.
  // See http://crbug.com/53899
  // TODO(jshin): we need to make sure our locale code matches that of
  // Firefox.
  DCHECK(!locale_.empty());
  base::FilePath locale_app_path = app_path.AppendASCII(locale_);
  base::FilePath default_locale_app_path = app_path.AppendASCII("en-US");
  if (base::DirectoryExists(locale_app_path))
    app_path = locale_app_path;
  else if (base::DirectoryExists(default_locale_app_path))
    app_path = default_locale_app_path;
#endif

  // Get search engine definition from file system.
  base::FileEnumerator engines(app_path, false, base::FileEnumerator::FILES);
  for (base::FilePath engine_path = engines.Next();
       !engine_path.value().empty(); engine_path = engines.Next()) {
    std::string file_data;
    base::ReadFileToString(file, &file_data);
    search_engine_data->push_back(file_data);
  }
}

void FirefoxImporter::GetSearchEnginesXMLDataFromJSON(
    std::vector<std::string>* search_engine_data) {
  // search-metadata.json contains keywords for search engines. This
  // file exists only if the user has set keywords for search engines.
  base::FilePath search_metadata_json_file =
      source_path_.AppendASCII("search-metadata.json");
  JSONFileValueDeserializer metadata_deserializer(search_metadata_json_file);
  std::unique_ptr<base::Value> metadata_root =
      metadata_deserializer.Deserialize(NULL, NULL);
  const base::DictionaryValue* search_metadata_root = NULL;
  if (metadata_root)
    metadata_root->GetAsDictionary(&search_metadata_root);

  // search.json contains information about search engines to import.
  base::FilePath search_json_file = source_path_.AppendASCII("search.json");
  if (!base::PathExists(search_json_file))
    return;

  JSONFileValueDeserializer deserializer(search_json_file);
  std::unique_ptr<base::Value> root = deserializer.Deserialize(NULL, NULL);
  const base::DictionaryValue* search_root = NULL;
  if (!root || !root->GetAsDictionary(&search_root))
    return;

  const std::string kDirectories("directories");
  const base::DictionaryValue* search_directories = NULL;
  if (!search_root->GetDictionary(kDirectories, &search_directories))
    return;

  // Dictionary |search_directories| contains a list of search engines
  // (default and installed). The list can be found from key <engines>
  // of the dictionary. Key <engines> is a grandchild of key <directories>.
  // However, key <engines> parent's key is dynamic which depends on
  // operating systems. For example,
  //   Ubuntu (for default search engine):
  //     /usr/lib/firefox/distribution/searchplugins/locale/en-US
  //   Ubuntu (for installed search engines):
  //     /home/<username>/.mozilla/firefox/lcd50n4n.default/searchplugins
  //   Windows (for default search engine):
  //     C:\\Program Files (x86)\\Mozilla Firefox\\browser\\searchplugins
  // Therefore, it needs to be retrieved by searching.

  for (base::DictionaryValue::Iterator it(*search_directories); !it.IsAtEnd();
       it.Advance()) {
    // The key of |it| may contains dot (.) which cannot be used as <key>
    // for retrieving <engines>. Hence, it is needed to get |it| as dictionary.
    // The resulted dictionary can be used for retrieving <engines>.
    const std::string kEngines("engines");
    const base::DictionaryValue* search_directory = NULL;
    if (!it.value().GetAsDictionary(&search_directory))
      continue;

    const base::ListValue* search_engines = NULL;
    if (!search_directory->GetList(kEngines, &search_engines))
      continue;

    const std::string kFilePath("filePath");
    const std::string kHidden("_hidden");
    for (size_t i = 0; i < search_engines->GetSize(); ++i) {
      const base::DictionaryValue* engine_info = NULL;
      if (!search_engines->GetDictionary(i, &engine_info))
        continue;

      bool is_hidden = false;
      std::string file_path;
      if (!engine_info->GetBoolean(kHidden, &is_hidden) ||
          !engine_info->GetString(kFilePath, &file_path))
        continue;

      if (!is_hidden) {
        const std::string kAppPrefix("[app]/");
        const std::string kProfilePrefix("[profile]/");
        base::FilePath xml_file = base::FilePath::FromUTF8Unsafe(file_path);

        // If |file_path| contains [app] or [profile] then they need to be
        // replaced with the actual app or profile path.
        size_t index = file_path.find(kAppPrefix);
        if (index != std::string::npos) {
          // Replace '[app]/' with actual app path.
          xml_file = app_path_.AppendASCII("searchplugins").AppendASCII(
              file_path.substr(index + kAppPrefix.length()));
        } else if ((index = file_path.find(kProfilePrefix)) !=
                   std::string::npos) {
          // Replace '[profile]/' with actual profile path.
          xml_file = source_path_.AppendASCII("searchplugins").AppendASCII(
              file_path.substr(index + kProfilePrefix.length()));
        }

        std::string file_data;
        base::ReadFileToString(xml_file, &file_data);

        // If a keyword is mentioned for this search engine, then add
        // it to the XML string as an <Alias> element and use this updated
        // string.
        const base::DictionaryValue* search_xml_path = NULL;
        if (search_metadata_root && search_metadata_root->HasKey(file_path) &&
            search_metadata_root->GetDictionaryWithoutPathExpansion(
                file_path, &search_xml_path)) {
          std::string alias;
          search_xml_path->GetString("alias", &alias);

          // Add <Alias> element as the last child element.
          size_t end_of_parent = file_data.find("</SearchPlugin>");
          if (end_of_parent != std::string::npos && !alias.empty())
            file_data.insert(end_of_parent, "<Alias>" + alias + "</Alias> \n");
        }
        search_engine_data->push_back(file_data);
      }
    }
  }
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
    NOTREACHED();
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
  sql::Statement s(db->GetUniqueStatement(query.c_str()));
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
    if (empty_folder != NULL)
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

  for (auto i = favicon_map.begin(); i != favicon_map.end(); ++i) {
    s.BindInt64(0, i->first);
    if (s.Step()) {
      std::vector<unsigned char> data;
      if (!s.ColumnBlobAsVector(1, &data))
        continue;

      favicon_base::FaviconUsageData usage_data;
      if (!SetFaviconData(s.ColumnString(0), data, &usage_data))
        continue;

      usage_data.urls = i->second;
      favicons->push_back(usage_data);
    }
    s.Reset(true);
  }
}

void FirefoxImporter::LoadFavicons(
    const std::vector<ImportedBookmarkEntry>& bookmarks,
    favicon_base::FaviconUsageDataList* favicons) {
  base::FilePath file = source_path_.AppendASCII("favicons.sqlite");
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
