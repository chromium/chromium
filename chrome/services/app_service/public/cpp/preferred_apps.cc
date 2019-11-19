// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/preferred_apps.h"

#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "chrome/services/app_service/public/cpp/intent_util.h"
#include "url/gurl.h"

namespace {

constexpr char kScheme[] = "scheme";
constexpr char kHost[] = "host";
constexpr char kPattern[] = "pattern";
constexpr char kAppId[] = "app_id";
constexpr char kLiteral[] = "literal";
constexpr char kPrefix[] = "prefix";
constexpr char kGlob[] = "glob";

const char* ConditionTypeToString(apps::mojom::ConditionType condition_type) {
  switch (condition_type) {
    case apps::mojom::ConditionType::kScheme:
      return kScheme;
    case apps::mojom::ConditionType::kHost:
      return kHost;
    case apps::mojom::ConditionType::kPattern:
      return kPattern;
  }
}

const char* MatchTypeToString(apps::mojom::PatternMatchType match_type) {
  switch (match_type) {
    case apps::mojom::PatternMatchType::kNone:
      return "";
    case apps::mojom::PatternMatchType::kLiteral:
      return kLiteral;
    case apps::mojom::PatternMatchType::kPrefix:
      return kPrefix;
    case apps::mojom::PatternMatchType::kGlob:
      return kGlob;
  }
}

apps::mojom::PatternMatchType StringToMatchType(const std::string& match_type) {
  if (match_type == kLiteral)
    return apps::mojom::PatternMatchType::kLiteral;
  else if (match_type == kPrefix)
    return apps::mojom::PatternMatchType::kPrefix;
  else if (match_type == kGlob)
    return apps::mojom::PatternMatchType::kGlob;
  else
    return apps::mojom::PatternMatchType::kNone;
}

base::Value* FindOrSetDictionary(base::Value* dict, base::StringPiece key) {
  auto* newDict = dict->FindKey(key);
  if (!newDict) {
    newDict = dict->SetKey(key, base::DictionaryValue());
  }
  return newDict;
}

// Find a match dictionary for a condition type and value pair. Return nullptr
// if cannot find a match.
// For example, to search for https scheme, we need to search "scheme" key
// first, then look for "https" key inside it. And for each condition pair,
// there is a possibility to have an "app_id" key. This will look
// like:
// ...
// {"scheme": {
//     "https": {
//        "app_id": <app_id>,
//        ... <other conditions, e.g. host>
//      }
//    }
//  }
// In this case this function will return the pointer to the dictionary that is
// the value of "https".
base::Value* FindDictionaryForTypeAndValue(
    base::Value* dict,
    apps::mojom::ConditionType condition_type,
    const std::string& value) {
  auto* condition_type_dict =
      dict->FindKey(ConditionTypeToString(condition_type));

  if (!condition_type_dict) {
    return nullptr;
  }

  if (condition_type != apps::mojom::ConditionType::kPattern) {
    return condition_type_dict->FindKey(value);
  }

  // For pattern matching, we need to go through all patterns and match types
  // to see if we have a match.
  // For example, if we have preferred apps set for same scheme, host, but
  // multiple patterns:
  // "/abc" literal match, "/abc" prefix match, "/g*h" glob match.
  // The representation is going to look like:
  // {"/abc": {
  //    "literal": {
  //      "app_id": <app_id>
  //     }
  //    "prefix": {
  //      "app_id": <app_id>
  //   }
  //  "/g*h": {
  //    "glob": {
  //      "app_id": <app_id>
  //     }
  //   }
  // }
  for (const auto& pattern_key_value : condition_type_dict->DictItems()) {
    const std::string& pattern = pattern_key_value.first;
    base::Value* pattern_dict = &pattern_key_value.second;
    DCHECK(pattern_dict);
    for (const auto& match_type_key_value : pattern_dict->DictItems()) {
      const std::string& match_type = match_type_key_value.first;
      if (apps_util::ConditionValueMatches(
              value, apps_util::MakeConditionValue(
                         pattern, StringToMatchType(match_type)))) {
        // Find the first match for patterns.
        // TODO(crbug.com/853604): Determine best match between patterns.
        return &match_type_key_value.second;
      }
    }
  }
  return nullptr;
}

// This method finds the dictionary keyed by the a condition_value and updates
// the |best_match_app_id| if there is a better app_id match found. For example,
// the input |dict| is pointing to the top layer of this dictionary, and what we
// found will be the inner dictionary key by https. Also in this case the
// |best_match_app_id| will be updated because there is a app id for this filter
// condition.
// ...
// {"scheme": {
//     "https": {
//        "app_id": <app_id>,
//        ... <other conditions, e.g. host>
//      }
//    }
//  }
base::Value* FindDictAndUpdateBestMatchAppId(
    apps::mojom::ConditionType condition_type,
    const std::string& value,
    base::Value* dict,
    base::Optional<std::string>* best_match_app_id) {
  auto* found_dict = FindDictionaryForTypeAndValue(dict, condition_type, value);
  if (!found_dict) {
    return found_dict;
  }
  std::string* app_id = found_dict->FindStringKey(kAppId);
  if (app_id) {
    *best_match_app_id = *app_id;
  }
  return found_dict;
}

// Going through each |condition| in the |conditions| by their |index|.
// Each |condition| can contain multiple |condition_values|, every combination
// between the |condition_values| in the |conditions| should be set for the
// app_id.
// For example, to set a preferred app for an |intent_filter| that support
// scheme http or https, and host www.google.com and www.google.com.au:
// conditions = [
//  {
//    condition_type: scheme
//    condition_values = [
//      {value: http, match_type: kNone},
//      {value: https, match_type: kNone}
//    ]
//  },
//  {
//    condition_type: host
//    condition_values = [
//      {value: www.google.com, match_type: kNone},
//      {value: www.google.com.au, match_type: kNone}
//    ]
//  }
// ]
//
// In this case, we should store the preferred app for this intent filter in
// this format:
// {"scheme": {
//    "http": {
//      "host": {
//        "www.google.com": {
//          "app_id": <app_id>
//         },
//        "www.google.com.au": {
//          "app_id": <app_id>
//         },
//       },
//    "https": {
//      "host": {
//        "www.google.com": {
//          "app_id": <app_id>
//         },
//        "www.google.com.au": {
//          "app_id": <app_id>
//         },
//       },
//     },
//   },
// }
void SetPreferredApp(const std::vector<apps::mojom::ConditionPtr>& conditions,
                     size_t index,
                     base::Value* dict,
                     const std::string& app_id) {
  // If there are no more condition key to add to the dictionary, we reach the
  // base case, set the key for the |app_id|.
  if (index == conditions.size()) {
    dict->SetStringKey(kAppId, app_id);
    return;
  }

  const auto& condition = conditions[index];
  auto* condition_type_dict = FindOrSetDictionary(
      dict, ConditionTypeToString(condition->condition_type));
  for (const auto& condition_value : condition->condition_values) {
    std::string condition_value_key = condition_value->value;
    base::Value* condition_value_dictionary;
    // For pattern type, use two nested dictionaries to represent the pattern
    // and the match type.
    if (condition->condition_type == apps::mojom::ConditionType::kPattern) {
      auto* match_type_dict =
          FindOrSetDictionary(condition_type_dict, condition_value_key);
      condition_value_dictionary = FindOrSetDictionary(
          match_type_dict, MatchTypeToString(condition_value->match_type));
    } else {
      condition_value_dictionary =
          FindOrSetDictionary(condition_type_dict, condition_value_key);
    }
    // For each |condition_value|, add dictionary for the following conditions.
    SetPreferredApp(conditions, index + 1, condition_value_dictionary, app_id);
  }
}

// Similar to SetPreferredApp(), this method go through every combination of
// the condition values to clear app_id.
void RemovePreferredApp(
    const std::vector<apps::mojom::ConditionPtr>& conditions,
    size_t index,
    base::Value* dict,
    const std::string& app_id) {
  // If there are no more condition key to add to the dictionary, we reach the
  // base case, delete the key_value pair if the stored app id is the same as
  // |app_id|.
  if (index == conditions.size()) {
    const std::string* app_id_found = dict->FindStringKey(kAppId);
    if (app_id_found && *app_id_found == app_id) {
      dict->RemoveKey(kAppId);
    }
    return;
  }

  const auto& condition = conditions[index];
  auto* condition_type_dict =
      dict->FindKey(ConditionTypeToString(condition->condition_type));
  if (!condition_type_dict) {
    return;
  }
  for (const auto& condition_value : condition->condition_values) {
    std::string condition_value_key = condition_value->value;
    base::Value* condition_value_dictionary;
    // For pattern type, use two nested dictionaries to represent the pattern
    // and the match type.
    if (condition->condition_type == apps::mojom::ConditionType::kPattern) {
      auto* match_type_dict = condition_type_dict->FindKey(condition_value_key);
      if (!match_type_dict) {
        continue;
      }
      condition_value_dictionary = match_type_dict->FindKey(
          MatchTypeToString(condition_value->match_type));
    } else {
      condition_value_dictionary =
          condition_type_dict->FindKey(condition_value_key);
    }
    if (!condition_value_dictionary) {
      continue;
    }
    // For each |condition_value|, search dictionary for the following
    // conditions.
    RemovePreferredApp(conditions, index + 1, condition_value_dictionary,
                       app_id);
    // Clean up empty dictionary after remove the app id.
    if (condition_value_dictionary->DictEmpty()) {
      condition_type_dict->RemoveKey(condition_value_key);
    }
  }
  // Clean up the empty dictionary if there is no content left.
  if (condition_type_dict->DictEmpty()) {
    dict->RemoveKey(ConditionTypeToString(condition->condition_type));
  }
}

}  // namespace

namespace apps {

PreferredApps::PreferredApps() = default;
PreferredApps::~PreferredApps() = default;

// static
// Recursively verifies that the structure of |value| matches what we expect.
//
// |value| should be a dictionary where each item is either:
// * key == kAppId and a string value, or
// * some other string value with a dictionary value.
bool PreferredApps::VerifyPreferredApps(base::Value* value) {
  if (!value->is_dict()) {
    return false;
  }
  bool all_items_valid = true;
  for (const auto& key_value : value->DictItems()) {
    bool item_valid = false;
    if (key_value.first == kAppId) {
      item_valid = key_value.second.is_string();
    } else {
      item_valid = VerifyPreferredApps(&key_value.second);
    }
    if (!item_valid) {
      all_items_valid = false;
      break;
    }
  }
  return all_items_valid;
}

// static
// Add a preferred app for a preferred app dictionary.
bool PreferredApps::AddPreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter,
    base::Value* preferred_apps) {
  if (!preferred_apps) {
    return false;
  }

  // For an |intent_filter| there could be multiple |conditions|, and for each
  // condition, there could be multiple |condition_values|. When we set
  // preferred app for and |intent_filter|, we need to add the preferred app for
  // all combinations of these |condition_values|.
  SetPreferredApp(intent_filter->conditions, 0, preferred_apps, app_id);
  return true;
}

// static
// Delete a preferred app for a preferred app dictionary.
bool PreferredApps::DeletePreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter,
    base::Value* preferred_apps) {
  if (!preferred_apps) {
    return false;
  }

  // For an |intent_filter| there could be multiple |conditions|, and for each
  // condition, there could be multiple |condition_values|. When we remove
  // preferred app for and |intent_filter|, we need to remove the preferred app
  // for all combinations of these |condition_values|.
  RemovePreferredApp(intent_filter->conditions, 0, preferred_apps, app_id);
  return true;
}

// static
void PreferredApps::DeleteAppId(const std::string& app_id,
                                base::Value* preferred_apps) {
  if (!preferred_apps) {
    return;
  }
  std::vector<std::string> keys_to_remove;
  for (const auto& key_value : preferred_apps->DictItems()) {
    if (key_value.first == kAppId) {
      if (key_value.second.GetString() == app_id) {
        keys_to_remove.push_back(kAppId);
      }
    } else {
      DeleteAppId(app_id, &key_value.second);
      if (key_value.second.DictEmpty()) {
        keys_to_remove.push_back(key_value.first);
      }
    }
  }
  for (const auto& key_to_remove : keys_to_remove) {
    preferred_apps->RemoveKey(key_to_remove);
  }
}

void PreferredApps::Init(std::unique_ptr<base::Value> preferred_apps) {
  if (preferred_apps && VerifyPreferredApps(preferred_apps.get())) {
    preferred_apps_ = std::move(preferred_apps);
  } else {
    preferred_apps_ =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  }
}

bool PreferredApps::AddPreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  if (!preferred_apps_) {
    return false;
  }
  return AddPreferredApp(app_id, intent_filter, preferred_apps_.get());
}

bool PreferredApps::DeletePreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  if (!preferred_apps_) {
    return false;
  }
  return DeletePreferredApp(app_id, intent_filter, preferred_apps_.get());
}

void PreferredApps::DeleteAppId(const std::string& app_id) {
  if (!preferred_apps_) {
    return;
  }
  DeleteAppId(app_id, preferred_apps_.get());
}

base::Optional<std::string> PreferredApps::FindPreferredAppForIntent(
    const apps::mojom::IntentPtr& intent) {
  base::Optional<std::string> best_match_app_id = base::nullopt;

  if (!preferred_apps_) {
    return best_match_app_id;
  }

  // Currently only support intent that has the full URL.
  if (!intent->scheme.has_value() || !intent->host.has_value() ||
      !intent->path.has_value()) {
    return best_match_app_id;
  }

  // The best match is the deepest layer the search can reach.
  // E.g. a preferred app for scheme and host is better match then preferred
  // app set for scheme only.
  auto* scheme_dict = FindDictAndUpdateBestMatchAppId(
      apps::mojom::ConditionType::kScheme, intent->scheme.value(),
      preferred_apps_.get(), &best_match_app_id);
  if (!scheme_dict) {
    return best_match_app_id;
  }

  auto* host_dict = FindDictAndUpdateBestMatchAppId(
      apps::mojom::ConditionType::kHost, intent->host.value(), scheme_dict,
      &best_match_app_id);
  if (!host_dict) {
    return best_match_app_id;
  }

  FindDictAndUpdateBestMatchAppId(apps::mojom::ConditionType::kPattern,
                                  intent->path.value(), host_dict,
                                  &best_match_app_id);
  return best_match_app_id;
}

base::Optional<std::string> PreferredApps::FindPreferredAppForUrl(
    const GURL& url) {
  auto intent = apps_util::CreateIntentFromUrl(url);
  return FindPreferredAppForIntent(intent);
}

base::Value PreferredApps::GetValue() {
  return preferred_apps_->Clone();
}

bool PreferredApps::IsInitialized() {
  return preferred_apps_ != nullptr;
}

}  // namespace apps
