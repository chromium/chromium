// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/content_bookmark_parser_utils.h"

#include <stddef.h>
#include <stdint.h>

#include "base/i18n/icu_string_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "components/user_data_importer/content/favicon_reencode.h"
#include "net/base/data_url.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace user_data_importer {

namespace {

static std::string stripDt(const std::string& lineDt) {
  // Remove "<DT>" if the line starts with "<DT>".  This may not occur if
  // "<DT>" was on the previous line.  Liberally accept entries that do not
  // have an opening "<DT>" at all.
  std::string line = lineDt;
  static const char kDtTag[] = "<DT>";
  if (base::StartsWith(line, kDtTag, base::CompareCase::INSENSITIVE_ASCII)) {
    line.erase(0, std::size(kDtTag) - 1);
    base::TrimString(line, " ", &line);
  }
  return line;
}

// Fetches the given `attribute` value from the `attribute_list`. Returns the
// value if successful.
std::optional<std::string> GetAttribute(const std::string& attribute_list,
                                        const std::string& attribute) {
  const char kQuote[] = "\"";

  size_t begin = attribute_list.find(attribute + "=" + kQuote);
  if (begin == std::string::npos) {
    return std::nullopt;  // Can't find the attribute.
  }

  begin += attribute.size() + 2;
  size_t end = begin + 1;

  while (end < attribute_list.size()) {
    if (attribute_list[end] == '"' && attribute_list[end - 1] != '\\') {
      break;
    }
    end++;
  }

  if (end == attribute_list.size()) {
    return std::nullopt;  // The value is not quoted.
  }

  return attribute_list.substr(begin, end - begin);
}

// Fetches a time attribute from the `attribute_list` and returns it as a
// base::Time.
std::optional<base::Time> GetTimeAttribute(const std::string& attribute_list,
                                           const std::string& attribute) {
  std::string value;
  std::optional<std::string> value_str =
      GetAttribute(attribute_list, attribute);
  if (value_str) {
    int64_t time;
    if (base::StringToInt64(*value_str, &time) && time > 0) {
      return base::Time::UnixEpoch() + base::Seconds(time);
    }
  }
  return std::nullopt;
}

// Fetches a UUID attribute from the `attribute_list` and returns it as a
// base::Uuid.
std::optional<base::Uuid> GetUuidAttribute(const std::string& attribute_list,
                                           const std::string& attribute) {
  std::optional<std::string> value = GetAttribute(attribute_list, attribute);
  if (value) {
    base::Uuid uuid = base::Uuid::ParseCaseInsensitive(*value);
    if (uuid.is_valid()) { return uuid; }
  }
  return std::nullopt;
}

// Fetches a boolean attribute from the `attribute_list` and returns it as a
// bool.
std::optional<bool> GetBoolAttribute(const std::string& attribute_list,
                                     const std::string& attribute) {
  std::optional<std::string> bool_value =
      GetAttribute(attribute_list, attribute);
  if (bool_value.has_value()) {
    if (bool_value.value() == "1") {
      return true;
    } else if (bool_value.value() == "0") {
      return false;
    }
  }
  return std::nullopt;
}

// Given the URL of a page and a favicon data URL, adds an appropriate record
// to the given favicon usage vector.
void DataURLToFaviconUsage(const GURL& link_url,
                           const GURL& favicon_data,
                           favicon_base::FaviconUsageDataList* favicons) {
  if (!link_url.is_valid() || !favicon_data.is_valid() ||
      !favicon_data.SchemeIs(url::kDataScheme)) {
    return;
  }

  // Parse the data URL.
  std::string mime_type, char_set, data;
  if (!net::DataURL::Parse(favicon_data, &mime_type, &char_set, &data) ||
      data.empty()) {
    return;
  }

  std::optional<std::vector<uint8_t>> png_data =
      importer::ReencodeFavicon(base::as_byte_span(data));
  if (!png_data) {
    return;  // Unable to decode.
  }

  favicon_base::FaviconUsageData usage;
  usage.png_data = std::move(png_data).value();

  // We need to make up a URL for the favicon. We use a version of the page's
  // URL so that we can be sure it will not collide.
  usage.favicon_url = GURL(std::string("made-up-favicon:") + link_url.spec());

  // We only have one URL per favicon for Firefox 2 bookmarks.
  usage.urls.insert(link_url);

  favicons->push_back(std::move(usage));
}

std::optional<std::string> ParseCharsetFromLine(const std::string& line) {
  if (!base::StartsWith(line, "<META", base::CompareCase::INSENSITIVE_ASCII) ||
      (line.find("CONTENT=\"") == std::string::npos &&
       line.find("content=\"") == std::string::npos)) {
    return std::nullopt;
  }

  const char kCharset[] = "charset=";
  size_t begin = line.find(kCharset);
  if (begin == std::string::npos) {
    return std::nullopt;
  }
  begin += sizeof(kCharset) - 1;
  size_t end = line.find_first_of('\"', begin);
  if (end == std::string::npos) {
    return std::nullopt;
  }
  return line.substr(begin, end - begin);
}

bool ParseFolderNameFromLine(const std::string& lineDt,
                             const std::string& charset,
                             std::u16string* folder_name,
                             bool* is_toolbar_folder,
                             base::Time* add_date,
                             std::optional<base::Uuid>* uuid,
                             std::optional<bool>* synced) {
  const char kFolderOpen[] = "<H3";
  const char kFolderClose[] = "</H3>";
  const char kToolbarFolderAttribute[] = "PERSONAL_TOOLBAR_FOLDER";
  const char kAddDateAttribute[] = "ADD_DATE";
  const char kUuidAttribute[] = "UUID";
  const char kSyncedAttribute[] = "SYNCED";

  std::string line = stripDt(lineDt);

  if (!base::StartsWith(line, kFolderOpen, base::CompareCase::SENSITIVE)) {
    return false;
  }

  size_t end = line.find(kFolderClose);
  size_t tag_end = line.rfind('>', end) + 1;
  // If no end tag or start tag is broken, we skip to find the folder name.
  if (end == std::string::npos || tag_end < std::size(kFolderOpen)) {
    return false;
  }

  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, folder_name);
  *folder_name = base::UnescapeForHTML(*folder_name);

  std::string attribute_list =
      line.substr(std::size(kFolderOpen), tag_end - std::size(kFolderOpen) - 1);
  std::string value;

  // Add date
  *add_date = GetTimeAttribute(attribute_list, kAddDateAttribute)
                  .value_or(base::Time::Now());

  // UUID.
  *uuid = GetUuidAttribute(attribute_list, kUuidAttribute);

  // SYNCED.
  *synced = GetBoolAttribute(attribute_list, kSyncedAttribute);

  std::optional<std::string> toolbar_attribute_value =
      GetAttribute(attribute_list, kToolbarFolderAttribute);
  if (toolbar_attribute_value &&
      base::EqualsCaseInsensitiveASCII(*toolbar_attribute_value, "true")) {
    *is_toolbar_folder = true;
  } else {
    *is_toolbar_folder = false;
  }

  return true;
}

bool ParseBookmarkFromLine(const std::string& lineDt,
                           const std::string& charset,
                           std::u16string* title,
                           GURL* url,
                           GURL* favicon,
                           std::u16string* shortcut,
                           base::Time* add_date,
                           std::optional<base::Time>* last_visit_date,
                           std::u16string* post_data,
                           std::optional<base::Uuid>* uuid,
                           std::optional<bool>* synced) {
  const char kItemOpen[] = "<A";
  const char kItemClose[] = "</A>";
  const char kFeedURLAttribute[] = "FEEDURL";
  const char kHrefAttribute[] = "HREF";
  const char kIconAttribute[] = "ICON";
  const char kShortcutURLAttribute[] = "SHORTCUTURL";
  const char kAddDateAttribute[] = "ADD_DATE";
  const char kLastVisitAttribute[] = "LAST_VISIT";
  const char kPostDataAttribute[] = "POST_DATA";
  const char kUuidAttribute[] = "UUID";
  const char kSyncedAttribute[] = "SYNCED";

  std::string line = stripDt(lineDt);
  title->clear();
  *url = GURL();
  *favicon = GURL();
  shortcut->clear();
  post_data->clear();
  *uuid = std::nullopt;
  *synced = std::nullopt;
  *add_date = base::Time::Now();
  *last_visit_date = std::nullopt;

  if (!base::StartsWith(line, kItemOpen, base::CompareCase::SENSITIVE)) {
    return false;
  }

  size_t end = line.find(kItemClose);
  size_t tag_end = line.rfind('>', end) + 1;
  if (end == std::string::npos || tag_end < std::size(kItemOpen)) {
    return false;  // No end tag or start tag is broken.
  }

  std::string attribute_list =
      line.substr(std::size(kItemOpen), tag_end - std::size(kItemOpen) - 1);

  // We don't import Live Bookmark folders, which is Firefox's RSS reading
  // feature, since the user never necessarily bookmarked them and we don't
  // have this feature to update their contents.
  if (GetAttribute(attribute_list, kFeedURLAttribute).has_value()) {
    return false;
  }

  // Title
  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, title);
  *title = base::UnescapeForHTML(*title);
  // URL is mandatory.
  std::optional<std::string> url_value =
      GetAttribute(attribute_list, kHrefAttribute);
  if (!url_value) {
    return false;
  }

  std::u16string url16;
  base::CodepageToUTF16(*url_value, charset.c_str(),
                        base::OnStringConversionError::SKIP, &url16);
  url16 = base::UnescapeForHTML(url16);
  *url = GURL(url16);

  // Favicon
  if (std::optional<std::string> icon =
          GetAttribute(attribute_list, kIconAttribute)) {
    *favicon = GURL(*icon);
  }

  // Keyword
  if (std::optional<std::string> shortcut_url =
          GetAttribute(attribute_list, kShortcutURLAttribute)) {
    base::CodepageToUTF16(*shortcut_url, charset.c_str(),
                          base::OnStringConversionError::SKIP, shortcut);
    *shortcut = base::UnescapeForHTML(*shortcut);
  }

  // Add date
  *add_date = GetTimeAttribute(attribute_list, kAddDateAttribute)
                  .value_or(base::Time::Now());

  // Last visit date
  *last_visit_date = GetTimeAttribute(attribute_list, kLastVisitAttribute);

  // Post data.
  if (std::optional<std::string> post_data_str =
          GetAttribute(attribute_list, kPostDataAttribute)) {
    base::CodepageToUTF16(*post_data_str, charset.c_str(),
                          base::OnStringConversionError::SKIP, post_data);
    *post_data = base::UnescapeForHTML(*post_data);
  }

  // UUID.
  *uuid = GetUuidAttribute(attribute_list, kUuidAttribute);

  // SYNCED.
  *synced = GetBoolAttribute(attribute_list, kSyncedAttribute);

  return true;
}

bool ParseMinimumBookmarkFromLine(const std::string& lineDt,
                                  const std::string& charset,
                                  std::u16string* title,
                                  GURL* url) {
  const char kItemOpen[] = "<A";
  const char kItemClose[] = "</";
  const char kHrefAttributeUpper[] = "HREF";
  const char kHrefAttributeLower[] = "href";

  std::string line = stripDt(lineDt);
  title->clear();
  *url = GURL();

  // Case-insensitive check of open tag.
  if (!base::StartsWith(line, kItemOpen,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }

  // Find any close tag.
  size_t end = line.find(kItemClose);
  size_t tag_end = line.rfind('>', end) + 1;
  if (end == std::string::npos || tag_end < std::size(kItemOpen)) {
    return false;  // No end tag or start tag is broken.
  }

  std::string attribute_list =
      line.substr(std::size(kItemOpen), tag_end - std::size(kItemOpen) - 1);

  // Title
  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, title);
  *title = base::UnescapeForHTML(*title);

  // URL is mandatory.
  std::optional<std::string> value =
      GetAttribute(attribute_list, kHrefAttributeUpper);
  if (!value) {
    value = GetAttribute(attribute_list, kHrefAttributeLower);
  }
  if (!value) {
    return false;
  }

  std::u16string url16;
  base::CodepageToUTF16(*value, charset.c_str(),
                        base::OnStringConversionError::SKIP, &url16);
  url16 = base::UnescapeForHTML(url16);
  *url = GURL(url16);

  return true;
}

}  // namespace

BookmarkParser::ParsedBookmarks ParseBookmarksUnsafe(
    const std::string& raw_html) {
  BookmarkParser::ParsedBookmarks parsing_result;

  std::vector<std::string> lines = base::SplitString(
      raw_html, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  std::u16string last_folder;
  bool last_folder_on_toolbar = false;
  bool last_folder_is_empty = true;
  bool has_subfolder = false;
  bool has_last_folder = false;
  base::Time last_folder_add_date;
  std::optional<base::Uuid> last_folder_uuid;
  std::optional<bool> last_folder_synced;
  std::vector<std::u16string> path;
  size_t toolbar_folder_index = 0;
  std::string charset = "UTF-8";  // If no charset is specified, assume utf-8.
  for (std::string& line : lines) {
    base::TrimString(line, " ", &line);

    // Remove "<HR>" if |line| starts with it. "<HR>" is the bookmark entries
    // separator in Firefox that Chrome does not support. Note that there can
    // be multiple "<HR>" tags at the beginning of a single line. See
    // http://crbug.com/257474.
    static const char kHrTag[] = "<HR>";
    while (
        base::StartsWith(line, kHrTag, base::CompareCase::INSENSITIVE_ASCII)) {
      line.erase(0, std::size(kHrTag) - 1);
      base::TrimString(line, " ", &line);
    }

    // Get the encoding of the bookmark file.
    if (std::optional<std::string> new_charset = ParseCharsetFromLine(line)) {
      charset = *new_charset;
      continue;
    }

    // Get the folder name.
    if (ParseFolderNameFromLine(line, charset, &last_folder,
                                &last_folder_on_toolbar, &last_folder_add_date,
                                &last_folder_uuid, &last_folder_synced)) {
      has_last_folder = true;
      continue;
    }

    // Get the bookmark entry.
    std::u16string title;
    std::u16string shortcut;
    GURL url, favicon;
    base::Time add_date;
    std::optional<base::Time> last_visit_date;
    std::u16string post_data;
    std::optional<base::Uuid> uuid;
    std::optional<bool> synced;
    bool is_bookmark;
    // TODO(crbug.com/40304654): We do not support POST based keywords yet.
    is_bookmark =
        ParseBookmarkFromLine(line, charset, &title, &url, &favicon, &shortcut,
                              &add_date, &last_visit_date, &post_data, &uuid,
                              &synced) ||
        ParseMinimumBookmarkFromLine(line, charset, &title, &url);

    // If bookmark contains a valid replaceable url and a keyword then import
    // it as search engine.
    std::string search_engine_url;
    if (is_bookmark && post_data.empty() &&
        CanImportURLAsSearchEngine(url, &search_engine_url) &&
        !shortcut.empty()) {
      user_data_importer::SearchEngineInfo search_engine_info;
      search_engine_info.url.assign(base::UTF8ToUTF16(search_engine_url));
      search_engine_info.keyword = shortcut;
      search_engine_info.display_name = title;
      parsing_result.search_engines.push_back(std::move(search_engine_info));
      continue;
    }

    if (is_bookmark) {
      last_folder_is_empty = false;
    }

    if (is_bookmark && post_data.empty()) {
      if (toolbar_folder_index > path.size() && !path.empty()) {
        NOTREACHED();  // error in parsing.
      }

      user_data_importer::ImportedBookmarkEntry entry;
      entry.creation_time = add_date;
      entry.last_visit_time = last_visit_date;
      entry.uuid = uuid;
      entry.synced = synced;
      entry.url = url;
      entry.title = title;

      if (toolbar_folder_index) {
        // The toolbar folder should be at the top level.
        entry.in_toolbar = true;
        entry.path.assign(path.begin() + toolbar_folder_index - 1, path.end());
      } else {
        // Add this bookmark to the list of |bookmarks|.
        if (!has_subfolder && has_last_folder) {
          path.push_back(last_folder);
          has_last_folder = false;
          last_folder.clear();
        }
        entry.path.assign(path.begin(), path.end());
      }
      parsing_result.bookmarks.push_back(std::move(entry));

      // Save the favicon. DataURLToFaviconUsage will handle the case where
      // there is no favicon.
      DataURLToFaviconUsage(url, favicon, &parsing_result.favicons);

      continue;
    }

    // Bookmarks in sub-folder are encapsulated with <DL> tag.
    if (base::StartsWith(line, "<DL>", base::CompareCase::INSENSITIVE_ASCII)) {
      has_subfolder = true;
      if (has_last_folder) {
        path.push_back(last_folder);
        has_last_folder = false;
        last_folder.clear();
      }
      if (last_folder_on_toolbar && !toolbar_folder_index) {
        toolbar_folder_index = path.size();
      }

      // Mark next folder empty as initial state.
      last_folder_is_empty = true;
    } else if (base::StartsWith(line, "</DL>",
                                base::CompareCase::INSENSITIVE_ASCII)) {
      if (path.empty()) {
        break;  // Mismatch <DL>.
      }

      std::u16string folder_title = path.back();
      path.pop_back();

      if (last_folder_is_empty) {
        // Empty folder should be added explicitly.
        user_data_importer::ImportedBookmarkEntry entry;
        entry.is_folder = true;
        entry.creation_time = last_folder_add_date;
        entry.title = folder_title;
        entry.uuid = last_folder_uuid;
        entry.synced = last_folder_synced;
        if (toolbar_folder_index) {
          // The toolbar folder should be at the top level.
          // Make sure we don't add the toolbar folder itself if it is empty.
          if (toolbar_folder_index <= path.size()) {
            entry.in_toolbar = true;
            entry.path.assign(path.begin() + toolbar_folder_index - 1,
                              path.end());
            parsing_result.bookmarks.push_back(std::move(entry));
          }
        } else {
          // Add this folder to the list of |bookmarks|.
          entry.path.assign(path.begin(), path.end());
          parsing_result.bookmarks.push_back(std::move(entry));
        }

        // Parent folder include current one, so it's not empty.
        last_folder_is_empty = false;
      }

      if (toolbar_folder_index > path.size()) {
        toolbar_folder_index = 0;
      }
    }
  }

  return parsing_result;
}

bool CanImportURLAsSearchEngine(const GURL& url,
                                std::string* search_engine_url) {
  std::string url_spec = url.possibly_invalid_spec();

  if (url_spec.empty()) {
    return false;
  }

  // Any occurrences of "%s" in the original URL string will have been escaped
  // as "%25s" by the GURL constructor. Restore them back to "%s".
  // Note: It is impossible to distinguish a literal "%25s" in the source
  // string from "%s". If the source string does contain "%25s", it will
  // unfortunately be converted to "%s" and erroneously used as a template.
  // See https://crbug.com/868214.
  base::ReplaceSubstringsAfterOffset(&url_spec, 0, "%25s", "%s");

  // Replace replacement terms ("%s") in |url_spec| with {searchTerms}.
  url_spec = TemplateURLRef::DisplayURLToURLRef(base::UTF8ToUTF16(url_spec));

  TemplateURLData data;
  data.SetURL(url_spec);
  *search_engine_url = url_spec;
  return TemplateURL(data).SupportsReplacement(SearchTermsData());
}

}  // namespace user_data_importer
