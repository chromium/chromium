// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/bookmark_html_reader.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/utility/importer/favicon_reencode.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "net/base/data_url.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// Fetches the given |attribute| value from the |attribute_list|. Returns true
// if successful, and |value| will contain the value.
bool GetAttribute(const std::string& attribute_list,
                  const std::string& attribute,
                  std::string* value) {
  const char kQuote[] = "\"";

  size_t begin = attribute_list.find(attribute + "=" + kQuote);
  if (begin == std::string::npos)
    return false;  // Can't find the attribute.

  begin += attribute.size() + 2;
  size_t end = begin + 1;

  while (end < attribute_list.size()) {
    if (attribute_list[end] == '"' &&
        attribute_list[end - 1] != '\\') {
      break;
    }
    end++;
  }

  if (end == attribute_list.size())
    return false;  // The value is not quoted.

  *value = attribute_list.substr(begin, end - begin);
  return true;
}

// Given the URL of a page and a favicon data URL, adds an appropriate record
// to the given favicon usage vector.
void DataURLToFaviconUsage(const GURL& link_url,
                           const GURL& favicon_data,
                           favicon_base::FaviconUsageDataList* favicons) {
  if (!link_url.is_valid() || !favicon_data.is_valid() ||
      !favicon_data.SchemeIs(url::kDataScheme))
    return;

  // Parse the data URL.
  std::string mime_type, char_set, data;
  if (!net::DataURL::Parse(favicon_data, &mime_type, &char_set, &data) ||
      data.empty())
    return;

  favicon_base::FaviconUsageData usage;
  if (!importer::ReencodeFavicon(
          reinterpret_cast<const unsigned char*>(&data[0]),
          data.size(), &usage.png_data))
    return;  // Unable to decode.

  // We need to make up a URL for the favicon. We use a version of the page's
  // URL so that we can be sure it will not collide.
  usage.favicon_url = GURL(std::string("made-up-favicon:") + link_url.spec());

  // We only have one URL per favicon for Firefox 2 bookmarks.
  usage.urls.insert(link_url);

  favicons->push_back(usage);
}

}  // namespace

namespace bookmark_html_reader {

static std::string stripDt(const std::string& lineDt) {
  // Remove "<DT>" if the line starts with "<DT>".  This may not occur if
  // "<DT>" was on the previous line.  Liberally accept entries that do not
  // have an opening "<DT>" at all.
  std::string line = lineDt;
  static const char kDtTag[] = "<DT>";
  if (base::StartsWith(line, kDtTag,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    line.erase(0, std::size(kDtTag) - 1);
    base::TrimString(line, " ", &line);
  }
  return line;
}

void ImportBookmarksFile(
    base::RepeatingCallback<bool(void)> cancellation_callback,
    base::RepeatingCallback<bool(const GURL&)> valid_url_callback,
    const base::FilePath& file_path,
    std::vector<ImportedBookmarkEntry>* bookmarks,
    std::vector<importer::SearchEngineInfo>* search_engines,
    favicon_base::FaviconUsageDataList* favicons) {
  std::string content;
  base::ReadFileToString(file_path, &content);
  std::vector<std::string> lines = base::SplitString(
      content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  std::u16string last_folder;
  bool last_folder_on_toolbar = false;
  bool last_folder_is_empty = true;
  bool has_subfolder = false;
  bool has_last_folder = false;
  base::Time last_folder_add_date;
  std::vector<std::u16string> path;
  size_t toolbar_folder_index = 0;
  std::string charset = "UTF-8";  // If no charset is specified, assume utf-8.
  for (size_t i = 0;
       i < lines.size() &&
           (cancellation_callback.is_null() || !cancellation_callback.Run());
       ++i) {
    std::string line;
    base::TrimString(lines[i], " ", &line);

    // Remove "<HR>" if |line| starts with it. "<HR>" is the bookmark entries
    // separator in Firefox that Chrome does not support. Note that there can be
    // multiple "<HR>" tags at the beginning of a single line.
    // See http://crbug.com/257474.
    static const char kHrTag[] = "<HR>";
    while (base::StartsWith(line, kHrTag,
                            base::CompareCase::INSENSITIVE_ASCII)) {
      line.erase(0, std::size(kHrTag) - 1);
      base::TrimString(line, " ", &line);
    }

    // Get the encoding of the bookmark file.
    if (internal::ParseCharsetFromLine(line, &charset))
      continue;

    // Get the folder name.
    if (internal::ParseFolderNameFromLine(line,
                                          charset,
                                          &last_folder,
                                          &last_folder_on_toolbar,
                                          &last_folder_add_date)) {
      has_last_folder = true;
      continue;
    }

    // Get the bookmark entry.
    std::u16string title;
    std::u16string shortcut;
    GURL url, favicon;
    base::Time add_date;
    std::u16string post_data;
    bool is_bookmark;
    // TODO(jcampan): http://b/issue?id=1196285 we do not support POST based
    //                keywords yet.
    is_bookmark =
        internal::ParseBookmarkFromLine(line, charset, &title,
                                        &url, &favicon, &shortcut,
                                        &add_date, &post_data) ||
        internal::ParseMinimumBookmarkFromLine(line, charset, &title, &url);

    // If bookmark contains a valid replaceable url and a keyword then import
    // it as search engine.
    std::string search_engine_url;
    if (is_bookmark && post_data.empty() &&
        CanImportURLAsSearchEngine(url, &search_engine_url) &&
            !shortcut.empty()) {
      importer::SearchEngineInfo search_engine_info;
      search_engine_info.url.assign(base::UTF8ToUTF16(search_engine_url));
      search_engine_info.keyword = shortcut;
      search_engine_info.display_name = title;
      search_engines->push_back(search_engine_info);
      continue;
    }

    if (is_bookmark)
      last_folder_is_empty = false;

    if (is_bookmark &&
        post_data.empty() &&
        (valid_url_callback.is_null() || valid_url_callback.Run(url))) {
      if (toolbar_folder_index > path.size() && !path.empty()) {
        NOTREACHED_IN_MIGRATION();  // error in parsing.
        break;
      }

      ImportedBookmarkEntry entry;
      entry.creation_time = add_date;
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
      bookmarks->push_back(entry);

      // Save the favicon. DataURLToFaviconUsage will handle the case where
      // there is no favicon.
      if (favicons)
        DataURLToFaviconUsage(url, favicon, favicons);

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
      if (last_folder_on_toolbar && !toolbar_folder_index)
        toolbar_folder_index = path.size();

      // Mark next folder empty as initial state.
      last_folder_is_empty = true;
    } else if (base::StartsWith(line, "</DL>",
                                base::CompareCase::INSENSITIVE_ASCII)) {
      if (path.empty())
        break;  // Mismatch <DL>.

      std::u16string folder_title = path.back();
      path.pop_back();

      if (last_folder_is_empty) {
        // Empty folder should be added explicitly.
        ImportedBookmarkEntry entry;
        entry.is_folder = true;
        entry.creation_time = last_folder_add_date;
        entry.title = folder_title;
        if (toolbar_folder_index) {
          // The toolbar folder should be at the top level.
          // Make sure we don't add the toolbar folder itself if it is empty.
          if (toolbar_folder_index <= path.size()) {
            entry.in_toolbar = true;
            entry.path.assign(path.begin() + toolbar_folder_index - 1,
                              path.end());
            bookmarks->push_back(entry);
          }
        } else {
          // Add this folder to the list of |bookmarks|.
          entry.path.assign(path.begin(), path.end());
          bookmarks->push_back(entry);
        }

        // Parent folder include current one, so it's not empty.
        last_folder_is_empty = false;
      }

      if (toolbar_folder_index > path.size())
        toolbar_folder_index = 0;
    }
  }
}

bool CanImportURLAsSearchEngine(const GURL& url,
                                std::string* search_engine_url) {
  std::string url_spec = url.possibly_invalid_spec();

  if (url_spec.empty())
    return false;

  // Any occurrences of "%s" in the original URL string will have been escaped
  // as "%25s" by the GURL constructor. Restore them back to "%s".
  // Note: It is impossible to distinguish a literal "%25s" in the source string
  // from "%s". If the source string does contain "%25s", it will unfortunately
  // be converted to "%s" and erroneously used as a template. See
  // https://crbug.com/868214.
  base::ReplaceSubstringsAfterOffset(&url_spec, 0, "%25s", "%s");

  // Replace replacement terms ("%s") in |url_spec| with {searchTerms}.
  url_spec =
      TemplateURLRef::DisplayURLToURLRef(base::UTF8ToUTF16(url_spec));

  TemplateURLData data;
  data.SetURL(url_spec);
  *search_engine_url = url_spec;
  return TemplateURL(data).SupportsReplacement(SearchTermsData());
}

namespace internal {

bool ParseCharsetFromLine(const std::string& line, std::string* charset) {
  if (!base::StartsWith(line, "<META", base::CompareCase::INSENSITIVE_ASCII) ||
      (line.find("CONTENT=\"") == std::string::npos &&
       line.find("content=\"") == std::string::npos)) {
    return false;
  }

  const char kCharset[] = "charset=";
  size_t begin = line.find(kCharset);
  if (begin == std::string::npos)
    return false;
  begin += sizeof(kCharset) - 1;
  size_t end = line.find_first_of('\"', begin);
  *charset = line.substr(begin, end - begin);
  return true;
}

bool ParseFolderNameFromLine(const std::string& lineDt,
                             const std::string& charset,
                             std::u16string* folder_name,
                             bool* is_toolbar_folder,
                             base::Time* add_date) {
  const char kFolderOpen[] = "<H3";
  const char kFolderClose[] = "</H3>";
  const char kToolbarFolderAttribute[] = "PERSONAL_TOOLBAR_FOLDER";
  const char kAddDateAttribute[] = "ADD_DATE";

  std::string line = stripDt(lineDt);

  if (!base::StartsWith(line, kFolderOpen, base::CompareCase::SENSITIVE))
    return false;

  size_t end = line.find(kFolderClose);
  size_t tag_end = line.rfind('>', end) + 1;
  // If no end tag or start tag is broken, we skip to find the folder name.
  if (end == std::string::npos || tag_end < std::size(kFolderOpen))
    return false;

  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, folder_name);
  *folder_name = base::UnescapeForHTML(*folder_name);

  std::string attribute_list =
      line.substr(std::size(kFolderOpen), tag_end - std::size(kFolderOpen) - 1);
  std::string value;

  // Add date
  if (GetAttribute(attribute_list, kAddDateAttribute, &value)) {
    int64_t time;
    base::StringToInt64(value, &time);
    // Upper bound it at 32 bits.
    if (0 < time && time < (1LL << 32))
      *add_date = base::Time::FromTimeT(time);
  }

  if (GetAttribute(attribute_list, kToolbarFolderAttribute, &value) &&
      base::EqualsCaseInsensitiveASCII(value, "true"))
    *is_toolbar_folder = true;
  else
    *is_toolbar_folder = false;

  return true;
}

bool ParseBookmarkFromLine(const std::string& lineDt,
                           const std::string& charset,
                           std::u16string* title,
                           GURL* url,
                           GURL* favicon,
                           std::u16string* shortcut,
                           base::Time* add_date,
                           std::u16string* post_data) {
  const char kItemOpen[] = "<A";
  const char kItemClose[] = "</A>";
  const char kFeedURLAttribute[] = "FEEDURL";
  const char kHrefAttribute[] = "HREF";
  const char kIconAttribute[] = "ICON";
  const char kShortcutURLAttribute[] = "SHORTCUTURL";
  const char kAddDateAttribute[] = "ADD_DATE";
  const char kPostDataAttribute[] = "POST_DATA";

  std::string line = stripDt(lineDt);
  title->clear();
  *url = GURL();
  *favicon = GURL();
  shortcut->clear();
  post_data->clear();
  *add_date = base::Time();

  if (!base::StartsWith(line, kItemOpen, base::CompareCase::SENSITIVE))
    return false;

  size_t end = line.find(kItemClose);
  size_t tag_end = line.rfind('>', end) + 1;
  if (end == std::string::npos || tag_end < std::size(kItemOpen))
    return false;  // No end tag or start tag is broken.

  std::string attribute_list =
      line.substr(std::size(kItemOpen), tag_end - std::size(kItemOpen) - 1);

  // We don't import Live Bookmark folders, which is Firefox's RSS reading
  // feature, since the user never necessarily bookmarked them and we don't
  // have this feature to update their contents.
  std::string value;
  if (GetAttribute(attribute_list, kFeedURLAttribute, &value))
    return false;

  // Title
  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, title);
  *title = base::UnescapeForHTML(*title);

  // URL
  if (GetAttribute(attribute_list, kHrefAttribute, &value)) {
    std::u16string url16;
    base::CodepageToUTF16(value, charset.c_str(),
                          base::OnStringConversionError::SKIP, &url16);
    url16 = base::UnescapeForHTML(url16);

    *url = GURL(url16);
  }

  // Favicon
  if (GetAttribute(attribute_list, kIconAttribute, &value))
    *favicon = GURL(value);

  // Keyword
  if (GetAttribute(attribute_list, kShortcutURLAttribute, &value)) {
    base::CodepageToUTF16(value, charset.c_str(),
                          base::OnStringConversionError::SKIP, shortcut);
    *shortcut = base::UnescapeForHTML(*shortcut);
  }

  // Add date
  if (GetAttribute(attribute_list, kAddDateAttribute, &value)) {
    int64_t time;
    base::StringToInt64(value, &time);
    // Upper bound it at 32 bits.
    if (0 < time && time < (1LL << 32))
      *add_date = base::Time::FromTimeT(time);
  }

  // Post data.
  if (GetAttribute(attribute_list, kPostDataAttribute, &value)) {
    base::CodepageToUTF16(value, charset.c_str(),
                          base::OnStringConversionError::SKIP, post_data);
    *post_data = base::UnescapeForHTML(*post_data);
  }

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
  if (!base::StartsWith(line, kItemOpen, base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // Find any close tag.
  size_t end = line.find(kItemClose);
  size_t tag_end = line.rfind('>', end) + 1;
  if (end == std::string::npos || tag_end < std::size(kItemOpen))
    return false;  // No end tag or start tag is broken.

  std::string attribute_list =
      line.substr(std::size(kItemOpen), tag_end - std::size(kItemOpen) - 1);

  // Title
  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, title);
  *title = base::UnescapeForHTML(*title);

  // URL
  std::string value;
  if (GetAttribute(attribute_list, kHrefAttributeUpper, &value) ||
      GetAttribute(attribute_list, kHrefAttributeLower, &value)) {
    if (charset.length() != 0) {
      std::u16string url16;
      base::CodepageToUTF16(value, charset.c_str(),
                            base::OnStringConversionError::SKIP, &url16);
      url16 = base::UnescapeForHTML(url16);

      *url = GURL(url16);
    } else {
      *url = GURL(value);
    }
  }

  return true;
}

}  // namespace internal

}  // namespace bookmark_html_reader
