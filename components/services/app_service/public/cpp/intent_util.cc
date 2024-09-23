// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_util.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace {

const char kWildCardAny[] = "*";
const char kMimeTypeSeparator[] = "/";
constexpr size_t kMimeTypeComponentSize = 2;
const char kAuthorityHostPortSeparator[] = ":";

const char kActionKey[] = "action";
const char kUrlKey[] = "url";
const char kMimeTypeKey[] = "mime_type";
const char kFileUrlsKey[] = "file_urls";
const char kActivityNameKey[] = "activity_name";
const char kDriveShareUrlKey[] = "drive_share_url";
const char kShareTextKey[] = "share_text";
const char kShareTitleKey[] = "share_title";
const char kStartTypeKey[] = "start_type";
const char kCategoriesKey[] = "categories";
const char kDataKey[] = "data";
const char kUiBypassedKey[] = "ui_bypassed";
const char kExtrasKey[] = "extras";
const char kMimeTypeInodeDirectory[] = "inode/directory";

bool ComponentMatched(const std::string& intent_component,
                      const std::string& filter_component) {
  return filter_component == kWildCardAny ||
         intent_component == filter_component;
}

}  // namespace

namespace apps_util {

const char kIntentActionMain[] = "main";
const char kIntentActionView[] = "view";
const char kIntentActionSend[] = "send";
const char kIntentActionSendMultiple[] = "send_multiple";
const char kIntentActionCreateNote[] = "create_note";
const char kIntentActionStartOnLockScreen[] = "start_on_lock_screen";
const char kIntentActionEdit[] = "edit";
const char kIntentActionPotentialFileHandler[] = "potential_file_handler";

const char kUseBrowserForLink[] = "use_browser";
const char kGuestOsActivityName[] = "open-with";

apps::IntentPtr MakeShareIntent(const std::vector<GURL>& filesystem_urls,
                                const std::vector<std::string>& mime_types) {
  auto intent = std::make_unique<apps::Intent>(filesystem_urls.size() == 1
                                                   ? kIntentActionSend
                                                   : kIntentActionSendMultiple);
  intent->mime_type = CalculateCommonMimeType(mime_types);

  DCHECK_EQ(filesystem_urls.size(), mime_types.size());
  for (size_t i = 0; i < filesystem_urls.size(); i++) {
    auto file = std::make_unique<apps::IntentFile>(filesystem_urls[i]);
    file->mime_type = mime_types.at(i);
    intent->files.push_back(std::move(file));
  }

  return intent;
}

apps::IntentPtr MakeShareIntent(const std::vector<GURL>& filesystem_urls,
                                const std::vector<std::string>& mime_types,
                                const std::string& text,
                                const std::string& title) {
  auto intent = MakeShareIntent(filesystem_urls, mime_types);
  if (!text.empty()) {
    intent->share_text = text;
  }
  if (!title.empty()) {
    intent->share_title = title;
  }
  return intent;
}

apps::IntentPtr MakeShareIntent(const GURL& filesystem_url,
                                const std::string& mime_type,
                                const GURL& drive_share_url,
                                bool is_directory) {
  auto intent = std::make_unique<apps::Intent>(kIntentActionSend);
  if (!is_directory) {
    intent->mime_type = mime_type;
    intent->files = std::vector<apps::IntentFilePtr>{};
    auto file = std::make_unique<apps::IntentFile>(filesystem_url);
    file->mime_type = mime_type;
    intent->files.push_back(std::move(file));
  }
  if (!drive_share_url.is_empty()) {
    intent->drive_share_url = drive_share_url;
  }
  return intent;
}

apps::IntentPtr MakeShareIntent(const std::string& text,
                                const std::string& title) {
  auto intent = std::make_unique<apps::Intent>(kIntentActionSend);
  intent->mime_type = "text/plain";
  intent->share_text = text;
  if (!title.empty()) {
    intent->share_title = title;
  }
  return intent;
}

apps::IntentPtr MakeShareIntent(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types,
    const std::vector<std::string>& dlp_source_urls) {
  auto intent = MakeShareIntent(filesystem_urls, mime_types);

  DCHECK_EQ(filesystem_urls.size(), dlp_source_urls.size());
  for (size_t i = 0; i < filesystem_urls.size(); i++) {
    intent->files[i]->dlp_source_url = dlp_source_urls[i];
  }
  return intent;
}

apps::IntentPtr MakeEditIntent(const GURL& filesystem_url,
                               const std::string& mime_type) {
  auto intent = std::make_unique<apps::Intent>(kIntentActionEdit);
  intent->mime_type = mime_type;

  auto file = std::make_unique<apps::IntentFile>(filesystem_url);
  file->mime_type = mime_type;
  intent->files.push_back(std::move(file));
  return intent;
}

apps::IntentPtr MakeIntentForActivity(const std::string& activity,
                                      const std::string& start_type,
                                      const std::string& category) {
  auto intent = std::make_unique<apps::Intent>(kIntentActionMain);
  intent->activity_name = activity;
  intent->start_type = start_type;
  intent->categories = std::vector<std::string>{category};
  return intent;
}

apps::IntentPtr CreateCreateNoteIntent() {
  return std::make_unique<apps::Intent>(kIntentActionCreateNote);
}

apps::IntentPtr CreateStartOnLockScreenIntent() {
  return std::make_unique<apps::Intent>(kIntentActionStartOnLockScreen);
}

bool ConditionValueMatches(std::string_view value,
                           const apps::ConditionValuePtr& condition_value) {
  return PatternMatchValue(value, condition_value->match_type,
                           condition_value->value);
}

bool PatternMatchValue(std::string_view test_value,
                       apps::PatternMatchType match_type,
                       std::string_view match_value) {
  switch (match_type) {
    case apps::PatternMatchType::kLiteral:
      return test_value == match_value;
    case apps::PatternMatchType::kPrefix:
      return base::StartsWith(test_value, match_value,
                              base::CompareCase::INSENSITIVE_ASCII);
    case apps::PatternMatchType::kSuffix:
      return base::EndsWith(test_value, match_value,
                            base::CompareCase::INSENSITIVE_ASCII);
    case apps::PatternMatchType::kGlob:
      return MatchGlob(test_value, match_value);
    case apps::PatternMatchType::kMimeType:
      // kMimeType as a match for kFile is handled in FileMatchesConditionValue.
      return MimeTypeMatched(test_value, match_value);
    case apps::PatternMatchType::kFileExtension:
    case apps::PatternMatchType::kIsDirectory: {
      // Handled in FileMatchesConditionValue.
      NOTREACHED_IN_MIGRATION();
      return false;
    }
  }
}

bool IsGenericFileHandler(const apps::IntentPtr& intent,
                          const apps::IntentFilterPtr& filter) {
  if (!intent || !filter || intent->files.empty()) {
    return false;
  }

  std::set<std::string> mime_types;
  std::set<std::string> file_extensions;
  filter->GetMimeTypesAndExtensions(mime_types, file_extensions);
  if (file_extensions.count("*") > 0 || mime_types.count("*") > 0 ||
      mime_types.count("*/*") > 0) {
    return true;
  }

  // If a text/* file handler matches with an unsupported text mime type, we
  // regard it as a generic match.
  if (mime_types.count("text/*")) {
    for (const auto& file : intent->files) {
      DCHECK(file);
      if (file->mime_type.has_value() &&
          blink::IsUnsupportedTextMimeType(file->mime_type.value())) {
        return true;
      }
    }
  }

  // If directory is selected, it is generic unless mime_types included
  // 'inode/directory'.
  for (const auto& file : intent->files) {
    DCHECK(file);
    if (file->is_directory.value_or(false)) {
      return mime_types.count(kMimeTypeInodeDirectory) == 0;
    }
  }
  return false;
}

bool MatchGlob(std::string_view value, std::string_view pattern) {
  static constexpr auto get_char = [](std::string_view s, size_t i) {
    if (i >= s.length()) [[unlikely]] {
      return '\0';
    }
    return s[i];
  };

  const size_t NP = pattern.length();
  const size_t NS = value.length();
  if (NP == 0) {
    return NS == 0;
  }
  size_t ip = 0, is = 0;
  char nextChar = get_char(pattern, 0);
  while (ip < NP && is < NS) {
    char c = nextChar;
    ++ip;
    nextChar = get_char(pattern, ip);
    const bool escaped = (c == '\\');
    if (escaped) {
      c = nextChar;
      ++ip;
      nextChar = get_char(pattern, ip);
    }
    if (nextChar == '*') {
      if (!escaped && c == '.') {
        if (ip >= (NP - 1)) {
          // At the end with a pattern match
          return true;
        }
        ++ip;
        nextChar = get_char(pattern, ip);
        // Consume everything until the next char in the pattern is found.
        if (nextChar == '\\') {
          ++ip;
          nextChar = get_char(pattern, ip);
        }
        do {
          if (get_char(value, is) == nextChar) {
            break;
          }
          ++is;
        } while (is < NS);
        if (is == NS) {
          // Next char in the pattern didn't exist in the match.
          return false;
        }
        ++ip;
        nextChar = get_char(pattern, ip);
        ++is;
      } else {
        // Consume only characters matching the one before '*'.
        do {
          if (get_char(value, is) != c) {
            break;
          }
          ++is;
        } while (is < NS);
        ++ip;
        nextChar = get_char(pattern, ip);
      }
    } else {
      if (c != '.' && get_char(value, is) != c) {
        return false;
      }
      ++is;
    }
  }

  if (ip >= NP && is >= NS) {
    // Reached the end of both strings
    return true;
  }

  // One last check: we may have finished the match string, but still have a
  // '.*' at the end of the pattern, which is still a match.
  if (ip == NP - 2 && get_char(pattern, ip) == '.' &&
      get_char(pattern, ip + 1) == '*') {
    return true;
  }

  return false;
}

bool MimeTypeMatched(std::string_view intent_mime_type,
                     std::string_view filter_mime_type) {
  std::vector<std::string> intent_components =
      base::SplitString(intent_mime_type, kMimeTypeSeparator,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<std::string> filter_components =
      base::SplitString(filter_mime_type, kMimeTypeSeparator,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (intent_components.size() > kMimeTypeComponentSize ||
      filter_components.size() > kMimeTypeComponentSize ||
      intent_components.size() == 0 || filter_components.size() == 0) {
    return false;
  }

  // If the filter component only contain main mime type, check if main type
  // matches.
  if (filter_components.size() == 1) {
    return ComponentMatched(intent_components[0], filter_components[0]);
  }

  // If the intent component only contain main mime type, complete the
  // mime type.
  if (intent_components.size() == 1) {
    intent_components.push_back(kWildCardAny);
  }

  // Both intent and intent filter can use wildcard for mime type.
  for (size_t i = 0; i < kMimeTypeComponentSize; i++) {
    if (!ComponentMatched(intent_components[i], filter_components[i])) {
      return false;
    }
  }
  return true;
}

bool ExtensionMatched(const std::string& file_name,
                      const std::string& filter_extension) {
  if (filter_extension == kWildCardAny)
    return true;

  // Normalise to have a preceding ".".
  std::string normalised_extension = filter_extension;
  if (filter_extension.length() > 0 && filter_extension[0] != '.') {
    normalised_extension = '.' + normalised_extension;
  }
  base::FilePath::StringType handler_extension =
      base::FilePath::FromUTF8Unsafe(normalised_extension).Extension();

  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(file_name);

  // Accept files whose extension or combined extension (e.g. ".tar.gz")
  // match the filter extension.
  return base::FilePath::CompareEqualIgnoreCase(handler_extension,
                                                file_path.Extension()) ||
         base::FilePath::CompareEqualIgnoreCase(handler_extension,
                                                file_path.FinalExtension());
}

base::Value ConvertIntentToValue(const apps::IntentPtr& intent) {
  base::Value::Dict intent_value;
  intent_value.Set(kActionKey, intent->action);

  if (intent->url.has_value()) {
    DCHECK(intent->url.value().is_valid());
    intent_value.Set(kUrlKey, intent->url.value().spec());
  }

  if (intent->mime_type.has_value() && !intent->mime_type.value().empty())
    intent_value.Set(kMimeTypeKey, intent->mime_type.value());

  if (!intent->files.empty()) {
    base::Value::List file_urls_list;
    for (const auto& file : intent->files) {
      DCHECK(file->url.is_valid());
      file_urls_list.Append(base::Value(file->url.spec()));
    }
    intent_value.Set(kFileUrlsKey, std::move(file_urls_list));
  }

  if (intent->activity_name.has_value() &&
      !intent->activity_name.value().empty()) {
    intent_value.Set(kActivityNameKey, intent->activity_name.value());
  }

  if (intent->drive_share_url.has_value()) {
    DCHECK(intent->drive_share_url.value().is_valid());
    intent_value.Set(kDriveShareUrlKey, intent->drive_share_url.value().spec());
  }

  if (intent->share_text.has_value() && !intent->share_text.value().empty())
    intent_value.Set(kShareTextKey, intent->share_text.value());

  if (intent->share_title.has_value() && !intent->share_title.value().empty())
    intent_value.Set(kShareTitleKey, intent->share_title.value());

  if (intent->start_type.has_value() && !intent->start_type.value().empty())
    intent_value.Set(kStartTypeKey, intent->start_type.value());

  if (!intent->categories.empty()) {
    base::Value::List categories;
    for (const auto& category : intent->categories) {
      categories.Append(category);
    }
    intent_value.Set(kCategoriesKey, std::move(categories));
  }

  if (intent->data.has_value() && !intent->data.value().empty())
    intent_value.Set(kDataKey, intent->data.value());

  if (intent->ui_bypassed.has_value()) {
    intent_value.Set(kUiBypassedKey, intent->ui_bypassed.value());
  }

  if (!intent->extras.empty()) {
    base::Value::Dict extras;
    for (const auto& extra : intent->extras) {
      extras.Set(extra.first, extra.second);
    }
    intent_value.Set(kExtrasKey, std::move(extras));
  }

  return base::Value(std::move(intent_value));
}

std::optional<std::string> GetStringValueFromDict(const base::Value::Dict& dict,
                                                  const std::string& key_name) {
  const base::Value* value = dict.Find(key_name);
  if (!value)
    return std::nullopt;

  const std::string* string_value = value->GetIfString();
  if (!string_value || string_value->empty())
    return std::nullopt;

  return *string_value;
}

std::optional<bool> GetBoolValueFromDict(const base::Value::Dict& dict,
                                         const std::string& key_name) {
  return dict.FindBool(key_name);
}

std::optional<GURL> GetGurlValueFromDict(const base::Value::Dict& dict,
                                         const std::string& key_name) {
  const std::string* url_spec = dict.FindString(key_name);
  if (!url_spec)
    return std::nullopt;

  GURL url(*url_spec);
  if (!url.is_valid())
    return std::nullopt;

  return url;
}

std::vector<apps::IntentFilePtr> GetFilesFromDict(const base::Value::Dict& dict,
                                                  const std::string& key_name) {
  const base::Value::List* value = dict.FindList(key_name);
  if (!value || value->empty())
    return std::vector<apps::IntentFilePtr>();

  std::vector<apps::IntentFilePtr> files;
  for (const auto& item : *value) {
    GURL url(item.GetString());
    if (url.is_valid()) {
      files.push_back(std::make_unique<apps::IntentFile>(url));
    }
  }
  return files;
}

std::vector<std::string> GetCategoriesFromDict(const base::Value::Dict& dict,
                                               const std::string& key_name) {
  const base::Value::List* value = dict.FindList(key_name);
  if (!value || value->empty())
    return std::vector<std::string>();

  std::vector<std::string> categories;
  for (const auto& item : *value)
    categories.push_back(item.GetString());

  return categories;
}

base::flat_map<std::string, std::string> GetExtrasFromDict(
    const base::Value::Dict& dict,
    const std::string& key_name) {
  const base::Value::Dict* value = dict.FindDict(key_name);
  if (!value)
    return base::flat_map<std::string, std::string>();

  base::flat_map<std::string, std::string> extras;
  for (auto pair : *value) {
    if (pair.second.is_string())
      extras[pair.first] = pair.second.GetString();
  }

  return extras;
}

apps::IntentPtr ConvertValueToIntent(base::Value&& value) {
  base::Value::Dict* dict = value.GetIfDict();
  if (!dict)
    return nullptr;

  return ConvertDictToIntent(*dict);
}

apps::IntentPtr ConvertDictToIntent(const base::Value::Dict& dict) {
  auto action = GetStringValueFromDict(dict, kActionKey);
  if (!action.has_value())
    return nullptr;
  auto intent = std::make_unique<apps::Intent>(action.value());
  intent->url = GetGurlValueFromDict(dict, kUrlKey);
  intent->mime_type = GetStringValueFromDict(dict, kMimeTypeKey);
  intent->files = GetFilesFromDict(dict, kFileUrlsKey);
  intent->activity_name = GetStringValueFromDict(dict, kActivityNameKey);
  intent->drive_share_url = GetGurlValueFromDict(dict, kDriveShareUrlKey);
  intent->share_text = GetStringValueFromDict(dict, kShareTextKey);
  intent->share_title = GetStringValueFromDict(dict, kShareTitleKey);
  intent->start_type = GetStringValueFromDict(dict, kStartTypeKey);
  intent->categories = GetCategoriesFromDict(dict, kCategoriesKey);
  intent->data = GetStringValueFromDict(dict, kDataKey);
  intent->ui_bypassed = GetBoolValueFromDict(dict, kUiBypassedKey);
  intent->extras = GetExtrasFromDict(dict, kExtrasKey);

  return intent;
}

std::string CalculateCommonMimeType(
    const std::vector<std::string>& mime_types) {
  const std::string any_mime_type = std::string(kWildCardAny) +
                                    std::string(kMimeTypeSeparator) +
                                    std::string(kWildCardAny);
  if (mime_types.size() == 0) {
    return any_mime_type;
  }

  std::vector<std::string> common_type =
      base::SplitString(mime_types[0], kMimeTypeSeparator,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (common_type.size() != 2) {
    return any_mime_type;
  }

  for (auto& mime_type : mime_types) {
    std::vector<std::string> type =
        base::SplitString(mime_type, kMimeTypeSeparator, base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (type.size() != kMimeTypeComponentSize) {
      return any_mime_type;
    }
    if (common_type[0] != type[0]) {
      return any_mime_type;
    }
    if (common_type[1] != type[1]) {
      common_type[1] = kWildCardAny;
    }
  }
  return common_type[0] + kMimeTypeSeparator + common_type[1];
}

SharedText ExtractSharedText(const std::string& share_text) {
  SharedText shared_text;
  std::string extracted_text = share_text;
  GURL extracted_url;
  size_t separator_pos = extracted_text.find_last_of(' ');
  size_t newline_pos = extracted_text.find_last_of('\n');
  if (newline_pos != std::string::npos &&
      (separator_pos == std::string::npos || separator_pos < newline_pos)) {
    separator_pos = newline_pos;
  }

  if (separator_pos == std::string::npos) {
    extracted_url = GURL(extracted_text);
    if (extracted_url.is_valid())
      extracted_text.clear();
  } else {
    extracted_url = GURL(extracted_text.substr(separator_pos + 1));
    if (extracted_url.is_valid())
      extracted_text.erase(separator_pos);
  }

  if (!extracted_text.empty())
    shared_text.text = extracted_text;

  if (extracted_url.is_valid())
    shared_text.url = extracted_url;

  return shared_text;
}

// static
std::optional<std::string> AuthorityView::PortToString(const GURL& url) {
  int port_number = url.EffectiveIntPort();
  if (port_number == url::PORT_UNSPECIFIED) {
    return std::nullopt;
  }
  return base::ToString(port_number);
}

// static
std::optional<std::string> AuthorityView::PortToString(
    const url::Origin& origin) {
  if (origin.port() == 0) {
    return std::nullopt;
  }
  return base::ToString(origin.port());
}

// static
AuthorityView AuthorityView::Decode(std::string_view encoded_string) {
  size_t i = encoded_string.find_last_of(kAuthorityHostPortSeparator);
  if (i == std::string_view::npos) {
    return {.host = encoded_string};
  }
  return {.host = encoded_string.substr(0, i),
          .port = encoded_string.substr(i + 1)};
}

// static
std::string AuthorityView::Encode(const GURL& url) {
  CHECK(url.is_valid());
  return AuthorityView{.host = url.host(), .port = PortToString(url)}.Encode();
}

// static
std::string AuthorityView::Encode(const url::Origin& origin) {
  CHECK(!origin.opaque());
  return AuthorityView{.host = origin.host(), .port = PortToString(origin)}
      .Encode();
}

std::string AuthorityView::Encode() {
  if (!port) {
    return std::string(host);
  }
  return base::StrCat({host, kAuthorityHostPortSeparator, *port});
}

}  // namespace apps_util
