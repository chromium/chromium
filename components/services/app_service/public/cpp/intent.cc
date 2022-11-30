// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent.h"

#include "base/files/safe_base_name.h"
#include "base/ranges/algorithm.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace apps {

IntentFile::IntentFile(const GURL& url) : url(url) {}

IntentFile::~IntentFile() = default;

bool IntentFile::operator==(const IntentFile& other) const {
  return url == other.url && mime_type == other.mime_type &&
         file_name == other.file_name && file_size == other.file_size &&
         is_directory == other.is_directory;
}

bool IntentFile::operator!=(const IntentFile& other) const {
  return !(*this == other);
}

std::unique_ptr<IntentFile> IntentFile::Clone() const {
  auto intent_file = std::make_unique<IntentFile>(url);
  if (mime_type.has_value()) {
    intent_file->mime_type = mime_type.value();
  }
  if (file_name.has_value()) {
    intent_file->file_name = file_name.value();
  }
  intent_file->file_size = file_size;
  intent_file->is_directory = is_directory;
  return intent_file;
}

bool IntentFile::MatchConditionValue(const ConditionValuePtr& condition_value) {
  switch (condition_value->match_type) {
    case PatternMatchType::kLiteral:
    case PatternMatchType::kPrefix:
    case PatternMatchType::kSuffix: {
      NOTREACHED();
      return false;
    }
    case PatternMatchType::kGlob: {
      return apps_util::MatchGlob(url.spec(), condition_value->value);
    }
    case PatternMatchType::kMimeType: {
      return mime_type.has_value() &&
             apps_util::MimeTypeMatched(mime_type.value(),
                                        condition_value->value);
    }
    case PatternMatchType::kFileExtension: {
      return apps_util::ExtensionMatched(url.ExtractFileName(),
                                         condition_value->value);
    }
    case PatternMatchType::kIsDirectory: {
      return is_directory.value_or(false);
    }
  }
}

bool IntentFile::MatchAnyConditionValue(
    const std::vector<ConditionValuePtr>& condition_values) {
  return base::ranges::any_of(condition_values,
                              [this](const ConditionValuePtr& condition_value) {
                                return MatchConditionValue(condition_value);
                              });
}

Intent::Intent(const std::string& action) : action(action) {}

Intent::Intent(const std::string& action, const GURL& url)
    : action(action), url(url) {}

Intent::Intent(const std::string& action, std::vector<IntentFilePtr> files)
    : action(action), files(std::move(files)) {}

Intent::~Intent() = default;

bool Intent::operator==(const Intent& other) const {
  for (int i = 0; i < static_cast<int>(files.size()); i++) {
    if ((*files[i]) != (*other.files[i])) {
      return false;
    }
  }

  return action == other.action && url == other.url &&
         mime_type == other.mime_type && activity_name == other.activity_name &&
         drive_share_url == other.drive_share_url &&
         share_text == other.share_text && share_title == other.share_title &&
         start_type == other.start_type && categories == other.categories &&
         data == other.data && ui_bypassed == other.ui_bypassed &&
         extras == other.extras;
}

bool Intent::operator!=(const Intent& other) const {
  return !(*this == other);
}

std::unique_ptr<Intent> Intent::Clone() const {
  auto intent = std::make_unique<Intent>(action);

  if (url.has_value()) {
    intent->url = url.value();
  }
  if (mime_type.has_value()) {
    intent->mime_type = mime_type.value();
  }
  for (const auto& file : files) {
    intent->files.push_back(file->Clone());
  }
  if (activity_name.has_value()) {
    intent->activity_name = activity_name.value();
  }
  if (drive_share_url.has_value()) {
    intent->drive_share_url = drive_share_url.value();
  }
  if (share_text.has_value()) {
    intent->share_text = share_text.value();
  }
  if (share_title.has_value()) {
    intent->share_title = share_title.value();
  }
  if (start_type.has_value()) {
    intent->start_type = start_type.value();
  }
  for (const auto& category : categories) {
    intent->categories.push_back(category);
  }
  if (data.has_value()) {
    intent->data = data.value();
  }
  intent->ui_bypassed = ui_bypassed;
  for (const auto& extra : extras) {
    intent->extras[extra.first] = extra.second;
  }
  return intent;
}

absl::optional<std::string> Intent::GetIntentConditionValueByType(
    ConditionType condition_type) {
  switch (condition_type) {
    case ConditionType::kAction: {
      return action;
    }
    case ConditionType::kScheme: {
      return url.has_value() ? absl::optional<std::string>(url->scheme())
                             : absl::nullopt;
    }
    case ConditionType::kHost: {
      return url.has_value() ? absl::optional<std::string>(url->host())
                             : absl::nullopt;
    }
    case ConditionType::kPath: {
      return url.has_value() ? absl::optional<std::string>(url->path())
                             : absl::nullopt;
    }
    case ConditionType::kMimeType: {
      return mime_type;
    }
    case ConditionType::kFile: {
      // Handled in IntentMatchesFileCondition.
      NOTREACHED();
      return absl::nullopt;
    }
  }
}

bool Intent::MatchFileCondition(const ConditionPtr& condition) {
  DCHECK_EQ(condition->condition_type, ConditionType::kFile);

  return !files.empty() &&
         base::ranges::all_of(files, [&condition](const IntentFilePtr& file) {
           return file->MatchAnyConditionValue(condition->condition_values);
         });
}

bool Intent::MatchCondition(const ConditionPtr& condition) {
  if (condition->condition_type == ConditionType::kFile) {
    return MatchFileCondition(condition);
  }

  absl::optional<std::string> value_to_match =
      GetIntentConditionValueByType(condition->condition_type);
  return value_to_match.has_value() &&
         base::ranges::any_of(condition->condition_values,
                              [&value_to_match](const auto& condition_value) {
                                return apps_util::ConditionValueMatches(
                                    value_to_match.value(), condition_value);
                              });
}

bool Intent::MatchFilter(const IntentFilterPtr& filter) {
  // Intent matches with this intent filter when all of the existing conditions
  // match.
  for (const auto& condition : filter->conditions) {
    if (!MatchCondition(condition)) {
      return false;
    }
  }
  return true;
}

bool Intent::IsShareIntent() {
  return action == apps_util::kIntentActionSend ||
         action == apps_util::kIntentActionSendMultiple;
}

bool Intent::OnlyShareToDrive() {
  return IsShareIntent() && drive_share_url && !share_text && files.empty();
}

bool Intent::IsIntentValid() {
  if (IsShareIntent()) {
    return share_text || !files.empty();
  }

  return true;
}

IntentFilePtr ConvertMojomIntentFileToIntentFile(
    const apps::mojom::IntentFilePtr& mojom_intent_file) {
  if (!mojom_intent_file) {
    return nullptr;
  }

  auto intent_file = std::make_unique<IntentFile>(mojom_intent_file->url);
  if (mojom_intent_file->mime_type.has_value()) {
    intent_file->mime_type = mojom_intent_file->mime_type.value();
  }
  if (mojom_intent_file->file_name.has_value()) {
    intent_file->file_name = mojom_intent_file->file_name.value();
  }
  intent_file->file_size = mojom_intent_file->file_size;
  intent_file->is_directory = GetOptionalBool(mojom_intent_file->is_directory);
  return intent_file;
}

apps::mojom::IntentFilePtr ConvertIntentFileToMojomIntentFile(
    const IntentFilePtr& intent_file) {
  if (!intent_file) {
    return nullptr;
  }

  auto mojom_intent_file = apps::mojom::IntentFile::New();
  mojom_intent_file->url = intent_file->url;
  if (intent_file->mime_type.has_value()) {
    mojom_intent_file->mime_type = intent_file->mime_type.value();
  }
  if (intent_file->file_name.has_value()) {
    mojom_intent_file->file_name = intent_file->file_name.value();
  }
  mojom_intent_file->file_size = intent_file->file_size;
  mojom_intent_file->is_directory =
      GetMojomOptionalBool(intent_file->is_directory);
  return mojom_intent_file;
}

IntentPtr ConvertMojomIntentToIntent(
    const apps::mojom::IntentPtr& mojom_intent) {
  if (!mojom_intent) {
    return nullptr;
  }

  auto intent = std::make_unique<Intent>(mojom_intent->action);

  if (mojom_intent->url.has_value()) {
    intent->url = mojom_intent->url.value();
  }
  if (mojom_intent->mime_type.has_value()) {
    intent->mime_type = mojom_intent->mime_type.value();
  }
  if (mojom_intent->files.has_value()) {
    for (const auto& file : mojom_intent->files.value()) {
      intent->files.push_back(ConvertMojomIntentFileToIntentFile(file));
    }
  }
  if (mojom_intent->activity_name.has_value()) {
    intent->activity_name = mojom_intent->activity_name.value();
  }
  if (mojom_intent->drive_share_url.has_value()) {
    intent->drive_share_url = mojom_intent->drive_share_url.value();
  }
  if (mojom_intent->share_text.has_value()) {
    intent->share_text = mojom_intent->share_text.value();
  }
  if (mojom_intent->share_title.has_value()) {
    intent->share_title = mojom_intent->share_title.value();
  }
  if (mojom_intent->start_type.has_value()) {
    intent->start_type = mojom_intent->start_type.value();
  }
  if (mojom_intent->categories.has_value()) {
    for (const auto& category : mojom_intent->categories.value()) {
      intent->categories.push_back(category);
    }
  }
  if (mojom_intent->data.has_value()) {
    intent->data = mojom_intent->data.value();
  }
  intent->ui_bypassed = GetOptionalBool(mojom_intent->ui_bypassed);
  if (mojom_intent->extras.has_value()) {
    for (const auto& extra : mojom_intent->extras.value()) {
      intent->extras[extra.first] = extra.second;
    }
  }
  return intent;
}

apps::mojom::IntentPtr ConvertIntentToMojomIntent(const IntentPtr& intent) {
  if (!intent) {
    return nullptr;
  }

  auto mojom_intent = apps::mojom::Intent::New();
  mojom_intent->action = intent->action;

  if (intent->url.has_value()) {
    mojom_intent->url = intent->url.value();
  }
  if (intent->mime_type.has_value()) {
    mojom_intent->mime_type = intent->mime_type.value();
  }
  if (!intent->files.empty()) {
    mojom_intent->files = std::vector<apps::mojom::IntentFilePtr>{};
    for (const auto& file : intent->files) {
      mojom_intent->files->push_back(ConvertIntentFileToMojomIntentFile(file));
    }
  }
  if (intent->activity_name.has_value()) {
    mojom_intent->activity_name = intent->activity_name.value();
  }
  if (intent->drive_share_url.has_value()) {
    mojom_intent->drive_share_url = intent->drive_share_url.value();
  }
  if (intent->share_text.has_value()) {
    mojom_intent->share_text = intent->share_text.value();
  }
  if (intent->share_title.has_value()) {
    mojom_intent->share_title = intent->share_title.value();
  }
  if (intent->start_type.has_value()) {
    mojom_intent->start_type = intent->start_type.value();
  }
  if (!intent->categories.empty()) {
    mojom_intent->categories = intent->categories;
  }
  if (intent->data.has_value()) {
    mojom_intent->data = intent->data.value();
  }
  mojom_intent->ui_bypassed = GetMojomOptionalBool(intent->ui_bypassed);
  if (!intent->extras.empty()) {
    mojom_intent->extras = intent->extras;
  }
  return mojom_intent;
}

}  // namespace apps
