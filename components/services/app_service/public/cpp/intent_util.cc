// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_util.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace {

const char kWildCardAny[] = "*";
const char kMimeTypeSeparator[] = "/";
constexpr size_t kMimeTypeComponentSize = 2;

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

// Get the field from the |intent| that need to be checked/matched based on
// |condition_type|.
// TODO(crbug.com/1253250): Remove this function after migrating to non-mojo
// AppService.
absl::optional<std::string> GetIntentConditionValueByType(
    apps::mojom::ConditionType condition_type,
    const apps::mojom::IntentPtr& intent) {
  switch (condition_type) {
    case apps::mojom::ConditionType::kAction:
      return intent->action;
    case apps::mojom::ConditionType::kScheme:
      return intent->url.has_value()
                 ? absl::optional<std::string>(intent->url->scheme())
                 : absl::nullopt;
    case apps::mojom::ConditionType::kHost:
      return intent->url.has_value()
                 ? absl::optional<std::string>(intent->url->host())
                 : absl::nullopt;
    case apps::mojom::ConditionType::kPattern:
      return intent->url.has_value()
                 ? absl::optional<std::string>(intent->url->path())
                 : absl::nullopt;
    case apps::mojom::ConditionType::kMimeType: {
      return intent->mime_type;
    }
    case apps::mojom::ConditionType::kFile: {
      // Handled in IntentMatchesFileCondition.
      NOTREACHED();
      return {};
    }
  }
}

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
const char kIntentActionEdit[] = "edit";

const char kUseBrowserForLink[] = "use_browser";

apps::mojom::IntentPtr CreateIntentFromUrl(const GURL& url) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionView;
  intent->url = url;
  return intent;
}

apps::mojom::IntentPtr CreateCreateNoteIntent() {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionCreateNote;
  return intent;
}

apps::mojom::IntentPtr CreateViewIntentFromFiles(
    std::vector<apps::mojom::IntentFilePtr> files) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionView;
  intent->files = std::move(files);
  return intent;
}

apps::mojom::IntentPtr CreateShareIntentFromFiles(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types) {
  DCHECK_EQ(filesystem_urls.size(), mime_types.size());
  auto intent = apps::mojom::Intent::New();
  intent->mime_type = CalculateCommonMimeType(mime_types);
  intent->files = std::vector<apps::mojom::IntentFilePtr>{};
  for (size_t i = 0; i < filesystem_urls.size(); i++) {
    auto file = apps::mojom::IntentFile::New();
    file->url = filesystem_urls[i];
    file->mime_type = mime_types.at(i);
    intent->files->push_back(std::move(file));
  }
  intent->action = filesystem_urls.size() == 1 ? kIntentActionSend
                                               : kIntentActionSendMultiple;
  return intent;
}

apps::mojom::IntentPtr CreateShareIntentFromFiles(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types,
    const std::string& share_text,
    const std::string& share_title) {
  auto intent = CreateShareIntentFromFiles(filesystem_urls, mime_types);
  if (!share_text.empty())
    intent->share_text = share_text;
  if (!share_title.empty())
    intent->share_title = share_title;
  return intent;
}

apps::mojom::IntentPtr CreateShareIntentFromDriveFile(
    const GURL& filesystem_url,
    const std::string& mime_type,
    const GURL& drive_share_url,
    bool is_directory) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionSend;
  if (!is_directory) {
    intent->mime_type = mime_type;
    intent->files = std::vector<apps::mojom::IntentFilePtr>{};
    auto file = apps::mojom::IntentFile::New();
    file->url = filesystem_url;
    intent->files->push_back(std::move(file));
  }
  if (!drive_share_url.is_empty()) {
    intent->drive_share_url = drive_share_url;
  }
  return intent;
}

apps::mojom::IntentPtr CreateShareIntentFromText(
    const std::string& share_text,
    const std::string& share_title) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionSend;
  intent->mime_type = "text/plain";
  intent->share_text = share_text;
  if (!share_title.empty())
    intent->share_title = share_title;
  return intent;
}

apps::mojom::IntentPtr CreateEditIntentFromFile(const GURL& filesystem_url,
                                                const std::string& mime_type) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionEdit;
  intent->files = std::vector<apps::mojom::IntentFilePtr>{};
  intent->mime_type = mime_type;

  auto file = apps::mojom::IntentFile::New();
  file->url = filesystem_url;
  file->mime_type = mime_type;
  intent->files->push_back(std::move(file));
  return intent;
}

apps::mojom::IntentPtr CreateIntentForActivity(const std::string& activity,
                                               const std::string& start_type,
                                               const std::string& category) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionMain;
  intent->activity_name = activity;
  intent->start_type = start_type;
  intent->categories = std::vector<std::string>{category};
  return intent;
}

bool ConditionValueMatches(const std::string& value,
                           const apps::ConditionValuePtr& condition_value) {
  switch (condition_value->match_type) {
    // Fallthrough as kNone and kLiteral has same matching type.
    case apps::PatternMatchType::kNone:
    case apps::PatternMatchType::kLiteral:
      return value == condition_value->value;
    case apps::PatternMatchType::kPrefix:
      return base::StartsWith(value, condition_value->value,
                              base::CompareCase::INSENSITIVE_ASCII);
    case apps::PatternMatchType::kSuffix:
      return base::EndsWith(value, condition_value->value,
                            base::CompareCase::INSENSITIVE_ASCII);
    case apps::PatternMatchType::kGlob:
      return MatchGlob(value, condition_value->value);
    case apps::PatternMatchType::kMimeType:
      // kMimeType as a match for kFile is handled in FileMatchesConditionValue.
      return MimeTypeMatched(value, condition_value->value);
    case apps::PatternMatchType::kFileExtension:
    case apps::PatternMatchType::kIsDirectory: {
      // Handled in FileMatchesConditionValue.
      NOTREACHED();
      return false;
    }
  }
}

bool ConditionValueMatches(
    const std::string& value,
    const apps::mojom::ConditionValuePtr& condition_value) {
  switch (condition_value->match_type) {
    // Fallthrough as kNone and kLiteral has same matching type.
    case apps::mojom::PatternMatchType::kNone:
    case apps::mojom::PatternMatchType::kLiteral:
      return value == condition_value->value;
    case apps::mojom::PatternMatchType::kPrefix:
      return base::StartsWith(value, condition_value->value,
                              base::CompareCase::INSENSITIVE_ASCII);
    case apps::mojom::PatternMatchType::kSuffix:
      return base::EndsWith(value, condition_value->value,
                            base::CompareCase::INSENSITIVE_ASCII);
    case apps::mojom::PatternMatchType::kGlob:
      return MatchGlob(value, condition_value->value);
    case apps::mojom::PatternMatchType::kMimeType:
      // kMimeType as a match for kFile is handled in FileMatchesConditionValue.
      return MimeTypeMatched(value, condition_value->value);
    case apps::mojom::PatternMatchType::kFileExtension:
    case apps::mojom::PatternMatchType::kIsDirectory: {
      // Handled in FileMatchesConditionValue.
      NOTREACHED();
      return false;
    }
  }
}

bool FileMatchesConditionValue(
    const apps::mojom::IntentFilePtr& file,
    const apps::mojom::ConditionValuePtr& condition_value) {
  switch (condition_value->match_type) {
    case apps::mojom::PatternMatchType::kNone:
    case apps::mojom::PatternMatchType::kLiteral:
    case apps::mojom::PatternMatchType::kPrefix:
    case apps::mojom::PatternMatchType::kSuffix:
      NOTREACHED();
      return false;
    case apps::mojom::PatternMatchType::kGlob:
      return MatchGlob(file->url.spec(), condition_value->value);
    case apps::mojom::PatternMatchType::kMimeType:
      return file->mime_type.has_value() &&
             MimeTypeMatched(file->mime_type.value(), condition_value->value);
    case apps::mojom::PatternMatchType::kFileExtension: {
      return ExtensionMatched(file->url.ExtractFileName(),
                              condition_value->value);
    }
    case apps::mojom::PatternMatchType::kIsDirectory:
      return file->is_directory == apps::mojom::OptionalBool::kTrue;
  }
}

bool FileMatchesAnyConditionValue(
    const apps::mojom::IntentFilePtr& file,
    const std::vector<apps::mojom::ConditionValuePtr>& condition_values) {
  return std::any_of(
      condition_values.begin(), condition_values.end(),
      [&file](const apps::mojom::ConditionValuePtr& condition_value) {
        return FileMatchesConditionValue(file, condition_value);
      });
}

bool IntentMatchesFileCondition(const apps::mojom::IntentPtr& intent,
                                const apps::mojom::ConditionPtr& condition) {
  DCHECK_EQ(condition->condition_type, apps::mojom::ConditionType::kFile);

  if (!intent->files.has_value() || intent->files->empty()) {
    return false;
  }

  return std::all_of(intent->files->begin(), intent->files->end(),
                     [&condition](const apps::mojom::IntentFilePtr& file) {
                       return FileMatchesAnyConditionValue(
                           file, condition->condition_values);
                     });
}

bool IntentMatchesCondition(const apps::mojom::IntentPtr& intent,
                            const apps::mojom::ConditionPtr& condition) {
  if (condition->condition_type == apps::mojom::ConditionType::kFile) {
    return IntentMatchesFileCondition(intent, condition);
  }

  absl::optional<std::string> value_to_match =
      GetIntentConditionValueByType(condition->condition_type, intent);
  if (!value_to_match.has_value()) {
    return false;
  }

  bool matched_any = std::any_of(
      condition->condition_values.begin(), condition->condition_values.end(),
      [&value_to_match](const auto& condition_value) {
        return ConditionValueMatches(value_to_match.value(), condition_value);
      });
  return matched_any;
}

bool IntentMatchesFilter(const apps::mojom::IntentPtr& intent,
                         const apps::mojom::IntentFilterPtr& filter) {
  // Intent matches with this intent filter when all of the existing conditions
  // match.
  for (const auto& condition : filter->conditions) {
    if (!IntentMatchesCondition(intent, condition)) {
      return false;
    }
  }
  return true;
}

bool FilterIsForFileExtensions(const apps::mojom::IntentFilterPtr& filter) {
  for (const auto& condition : filter->conditions) {
    // We expect action conditions to be paired with file conditions.
    if (condition->condition_type == apps::mojom::ConditionType::kAction) {
      continue;
    }
    if (condition->condition_type != apps::mojom::ConditionType::kFile) {
      return false;
    }
    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->match_type !=
          apps::mojom::PatternMatchType::kFileExtension) {
        return false;
      }
    }
  }
  return true;
}

namespace {

void GetMimeTypesAndExtensions(const apps::mojom::IntentFilterPtr& filter,
                               std::set<std::string>& mime_types,
                               std::set<std::string>& file_extensions) {
  for (const auto& condition : filter->conditions) {
    if (condition->condition_type != apps::mojom::ConditionType::kFile) {
      continue;
    }
    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->match_type ==
          apps::mojom::PatternMatchType::kFileExtension) {
        file_extensions.insert(condition_value->value);
      }
      if (condition_value->match_type ==
          apps::mojom::PatternMatchType::kMimeType) {
        mime_types.insert(condition_value->value);
      }
    }
  }
}

}  // namespace

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

bool IsGenericFileHandler(const apps::mojom::IntentPtr& intent,
                          const apps::mojom::IntentFilterPtr& filter) {
  if (!intent->files.has_value())
    return false;

  std::set<std::string> mime_types;
  std::set<std::string> file_extensions;
  GetMimeTypesAndExtensions(filter, mime_types, file_extensions);
  if (file_extensions.count("*") > 0 || mime_types.count("*") > 0 ||
      mime_types.count("*/*") > 0)
    return true;

  // If a text/* file handler matches with an unsupported text mime type, we
  // regard it as a generic match.
  if (mime_types.count("text/*")) {
    for (const auto& file : intent->files.value()) {
      if (file->mime_type.has_value() &&
          blink::IsUnsupportedTextMimeType(file->mime_type.value())) {
        return true;
      }
    }
  }

  // If directory is selected, it is generic unless mime_types included
  // 'inode/directory'.
  for (const auto& file : intent->files.value()) {
    if (file->is_directory == apps::mojom::OptionalBool::kTrue)
      return mime_types.count(kMimeTypeInodeDirectory) == 0;
  }
  return false;
}

bool IsShareIntent(const apps::mojom::IntentPtr& intent) {
  return intent->action == kIntentActionSend ||
         intent->action == kIntentActionSendMultiple;
}

bool MatchGlob(const std::string& value, const std::string& pattern) {
#define GET_CHAR(s, i) ((UNLIKELY(i >= s.length())) ? '\0' : s[i])

  const size_t NP = pattern.length();
  const size_t NS = value.length();
  if (NP == 0) {
    return NS == 0;
  }
  size_t ip = 0, is = 0;
  char nextChar = GET_CHAR(pattern, 0);
  while (ip < NP && is < NS) {
    char c = nextChar;
    ++ip;
    nextChar = GET_CHAR(pattern, ip);
    const bool escaped = (c == '\\');
    if (escaped) {
      c = nextChar;
      ++ip;
      nextChar = GET_CHAR(pattern, ip);
    }
    if (nextChar == '*') {
      if (!escaped && c == '.') {
        if (ip >= (NP - 1)) {
          // At the end with a pattern match
          return true;
        }
        ++ip;
        nextChar = GET_CHAR(pattern, ip);
        // Consume everything until the next char in the pattern is found.
        if (nextChar == '\\') {
          ++ip;
          nextChar = GET_CHAR(pattern, ip);
        }
        do {
          if (GET_CHAR(value, is) == nextChar) {
            break;
          }
          ++is;
        } while (is < NS);
        if (is == NS) {
          // Next char in the pattern didn't exist in the match.
          return false;
        }
        ++ip;
        nextChar = GET_CHAR(pattern, ip);
        ++is;
      } else {
        // Consume only characters matching the one before '*'.
        do {
          if (GET_CHAR(value, is) != c) {
            break;
          }
          ++is;
        } while (is < NS);
        ++ip;
        nextChar = GET_CHAR(pattern, ip);
      }
    } else {
      if (c != '.' && GET_CHAR(value, is) != c)
        return false;
      ++is;
    }
  }

  if (ip >= NP && is >= NS) {
    // Reached the end of both strings
    return true;
  }

  // One last check: we may have finished the match string, but still have a
  // '.*' at the end of the pattern, which is still a match.
  if (ip == NP - 2 && GET_CHAR(pattern, ip) == '.' &&
      GET_CHAR(pattern, ip + 1) == '*') {
    return true;
  }

  return false;

#undef GET_CHAR
}

bool MimeTypeMatched(const std::string& intent_mime_type,
                     const std::string& filter_mime_type) {
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

bool OnlyShareToDrive(const apps::mojom::IntentPtr& intent) {
  return IsShareIntent(intent) && intent->drive_share_url &&
         !intent->share_text && !intent->files;
}

bool IsIntentValid(const apps::mojom::IntentPtr& intent) {
  // TODO(crbug.com/853604):Add more checks here to make this a general intent
  // validity check. Return false if this is a share intent with no file or
  // text.
  if (IsShareIntent(intent))
    return intent->share_text || intent->files;

  return true;
}

base::Value ConvertIntentToValue(const apps::mojom::IntentPtr& intent) {
  base::Value intent_value(base::Value::Type::DICTIONARY);
  intent_value.SetStringKey(kActionKey, intent->action);

  if (intent->url.has_value()) {
    DCHECK(intent->url.value().is_valid());
    intent_value.SetStringKey(kUrlKey, intent->url.value().spec());
  }

  if (intent->mime_type.has_value() && !intent->mime_type.value().empty())
    intent_value.SetStringKey(kMimeTypeKey, intent->mime_type.value());

  if (intent->files.has_value() && !intent->files.value().empty()) {
    base::Value file_urls_list(base::Value::Type::LIST);
    for (const auto& file : intent->files.value()) {
      DCHECK(file->url.is_valid());
      file_urls_list.Append(base::Value(file->url.spec()));
    }
    intent_value.SetKey(kFileUrlsKey, std::move(file_urls_list));
  }

  if (intent->activity_name.has_value() &&
      !intent->activity_name.value().empty()) {
    intent_value.SetStringKey(kActivityNameKey, intent->activity_name.value());
  }

  if (intent->drive_share_url.has_value()) {
    DCHECK(intent->drive_share_url.value().is_valid());
    intent_value.SetStringKey(kDriveShareUrlKey,
                              intent->drive_share_url.value().spec());
  }

  if (intent->share_text.has_value() && !intent->share_text.value().empty())
    intent_value.SetStringKey(kShareTextKey, intent->share_text.value());

  if (intent->share_title.has_value() && !intent->share_title.value().empty())
    intent_value.SetStringKey(kShareTitleKey, intent->share_title.value());

  if (intent->start_type.has_value() && !intent->start_type.value().empty())
    intent_value.SetStringKey(kStartTypeKey, intent->start_type.value());

  if (intent->categories.has_value() && !intent->categories.value().empty()) {
    base::Value categories(base::Value::Type::LIST);
    for (const auto& category : intent->categories.value()) {
      categories.Append(base::Value(category));
    }
    intent_value.SetKey(kCategoriesKey, std::move(categories));
  }

  if (intent->data.has_value() && !intent->data.value().empty())
    intent_value.SetStringKey(kDataKey, intent->data.value());

  if (intent->ui_bypassed != apps::mojom::OptionalBool::kUnknown) {
    intent_value.SetBoolKey(
        kUiBypassedKey,
        intent->ui_bypassed == apps::mojom::OptionalBool::kTrue ? true : false);
  }

  if (intent->extras.has_value() && !intent->extras.value().empty()) {
    base::Value extras(base::Value::Type::DICTIONARY);
    for (const auto& extra : intent->extras.value()) {
      extras.SetStringKey(extra.first, extra.second);
    }
    intent_value.SetKey(kExtrasKey, std::move(extras));
  }

  return intent_value;
}

absl::optional<std::string> GetStringValueFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  const base::Value* value = dict.FindKey(key_name);
  if (!value)
    return absl::nullopt;

  const std::string* string_value = value->GetIfString();
  if (!string_value || string_value->empty())
    return absl::nullopt;

  return *string_value;
}

apps::mojom::OptionalBool GetBoolValueFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  absl::optional<bool> value = dict.FindBoolKey(key_name);
  if (!value.has_value())
    return apps::mojom::OptionalBool::kUnknown;

  return value.value() ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;
}

absl::optional<GURL> GetGurlValueFromDict(const base::DictionaryValue& dict,
                                          const std::string& key_name) {
  const std::string* url_spec = dict.FindStringKey(key_name);
  if (!url_spec)
    return absl::nullopt;

  GURL url(*url_spec);
  if (!url.is_valid())
    return absl::nullopt;

  return url;
}

absl::optional<std::vector<apps::mojom::IntentFilePtr>> GetFilesFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  const base::Value* value = dict.FindListKey(key_name);
  if (!value || !value->is_list() || value->GetListDeprecated().empty())
    return absl::nullopt;

  std::vector<apps::mojom::IntentFilePtr> files;
  for (const auto& item : value->GetListDeprecated()) {
    GURL url(item.GetString());
    if (url.is_valid()) {
      auto file = apps::mojom::IntentFile::New();
      file->url = std::move(url);
      files.push_back(std::move(file));
    }
  }
  return files;
}

absl::optional<std::vector<std::string>> GetCategoriesFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  const base::Value* value = dict.FindListKey(key_name);
  if (!value || !value->is_list() || value->GetListDeprecated().empty())
    return absl::nullopt;

  std::vector<std::string> categories;
  for (const auto& item : value->GetListDeprecated())
    categories.push_back(item.GetString());

  return categories;
}

absl::optional<base::flat_map<std::string, std::string>> GetExtrasFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  const base::Value* value = dict.FindDictKey(key_name);
  if (!value || !value->is_dict())
    return absl::nullopt;

  base::flat_map<std::string, std::string> extras;
  for (auto pair : value->DictItems()) {
    if (pair.second.is_string())
      extras[pair.first] = pair.second.GetString();
  }

  return extras;
}

apps::mojom::IntentPtr ConvertValueToIntent(base::Value&& value) {
  auto intent = apps::mojom::Intent::New();

  base::DictionaryValue* dict = nullptr;
  if (!value.is_dict() || !value.GetAsDictionary(&dict))
    return intent;

  auto action = GetStringValueFromDict(*dict, kActionKey);
  if (!action.has_value())
    return intent;
  intent->action = action.value();
  intent->url = GetGurlValueFromDict(*dict, kUrlKey);
  intent->mime_type = GetStringValueFromDict(*dict, kMimeTypeKey);
  intent->files = GetFilesFromDict(*dict, kFileUrlsKey);
  intent->activity_name = GetStringValueFromDict(*dict, kActivityNameKey);
  intent->drive_share_url = GetGurlValueFromDict(*dict, kDriveShareUrlKey);
  intent->share_text = GetStringValueFromDict(*dict, kShareTextKey);
  intent->share_title = GetStringValueFromDict(*dict, kShareTitleKey);
  intent->start_type = GetStringValueFromDict(*dict, kStartTypeKey);
  intent->categories = GetCategoriesFromDict(*dict, kCategoriesKey);
  intent->data = GetStringValueFromDict(*dict, kDataKey);
  intent->ui_bypassed = GetBoolValueFromDict(*dict, kUiBypassedKey);
  intent->extras = GetExtrasFromDict(*dict, kExtrasKey);

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

}  // namespace apps_util
