// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_util.h"

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

const char kWildCardAny[] = "*";
const char kMimeTypeSeparator[] = "/";
constexpr size_t kMimeTypeComponentSize = 2;

// Get the intent condition value based on the condition type.
base::Optional<std::string> GetIntentConditionValueByType(
    apps::mojom::ConditionType condition_type,
    const apps::mojom::IntentPtr& intent) {
  switch (condition_type) {
    case apps::mojom::ConditionType::kAction:
      return intent->action;
    case apps::mojom::ConditionType::kScheme:
      return intent->url.has_value()
                 ? base::Optional<std::string>(intent->url->scheme())
                 : base::nullopt;
    case apps::mojom::ConditionType::kHost:
      return intent->url.has_value()
                 ? base::Optional<std::string>(intent->url->host())
                 : base::nullopt;
    case apps::mojom::ConditionType::kPattern:
      return intent->url.has_value()
                 ? base::Optional<std::string>(intent->url->path())
                 : base::nullopt;
    case apps::mojom::ConditionType::kMimeType:
      return intent->mime_type;
  }
}

bool ComponentMatched(const std::string& intent_component,
                      const std::string& filter_component) {
  return filter_component == kWildCardAny ||
         intent_component == filter_component;
}

// TODO(crbug.com/1092784): Handle file path with extension with mime type.
// Unlike Android mime type matching logic, if the intent mime type has *, it
// can only match with *, not anything. The reason for this is the way we find
// the common mime type for multiple files. It uses * to represent more than one
// types in the list, which will cause an issue if we treat that as we want to
// match with any filter. e.g. If we select a .zip, .jep and a .txt, the common
// mime type will be */*, with Android matching logic, it will match with filter
// that has mime type video, which is not what we expected.
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

// Calculates the least general mime type that matches all of the given ones.
// E.g., for ["image/jpeg", "image/png"] it will be "image/*". ["text/html",
// "text/html"] will return "text/html", and ["text/html", "image/jpeg"]
// becomes the fully wildcard pattern.
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

}  // namespace

namespace apps_util {

const char kIntentActionView[] = "view";
const char kIntentActionSend[] = "send";
const char kIntentActionSendMultiple[] = "send_multiple";

apps::mojom::IntentPtr CreateIntentFromUrl(const GURL& url) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionView;
  intent->url = url;
  return intent;
}

apps::mojom::IntentPtr CreateShareIntentFromFiles(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types) {
  auto intent = apps::mojom::Intent::New();
  intent->action = filesystem_urls.size() == 1 ? kIntentActionSend
                                               : kIntentActionSendMultiple;
  intent->mime_type = CalculateCommonMimeType(mime_types);
  intent->file_urls = filesystem_urls;
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
    intent->file_urls = std::vector<GURL>{filesystem_url};
  }
  if (!drive_share_url.is_empty()) {
    intent->drive_share_url = drive_share_url;
  }
  return intent;
}

apps::mojom::IntentPtr CreateShareIntentFromText(
    const std::string& share_text) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionSend;
  intent->mime_type = "text/plain";
  intent->share_text = share_text;
  return intent;
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
    case apps::mojom::PatternMatchType::kGlob:
      return MatchGlob(value, condition_value->value);
    case apps::mojom::PatternMatchType::kMimeType:
      return MimeTypeMatched(value, condition_value->value);
  }
}

bool IntentMatchesCondition(const apps::mojom::IntentPtr& intent,
                            const apps::mojom::ConditionPtr& condition) {
  base::Optional<std::string> value_to_match =
      GetIntentConditionValueByType(condition->condition_type, intent);
  if (!value_to_match.has_value()) {
    return false;
  }
  for (const auto& condition_value : condition->condition_values) {
    if (ConditionValueMatches(value_to_match.value(), condition_value)) {
      return true;
    }
  }
  return false;
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

// TODO(crbug.com/853604): For glob match, it is currently only for Android
// intent filters, so we will use the ARC intent filter implementation that is
// transcribed from Android codebase, to prevent divergence from Android code.
// This is now also used for mime type matching.
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

bool OnlyShareToDrive(const apps::mojom::IntentPtr& intent) {
  if (intent->action == kIntentActionSend ||
      intent->action == kIntentActionSendMultiple) {
    if (intent->drive_share_url.has_value() &&
        !intent->share_text.has_value() && !intent->file_urls.has_value()) {
      return true;
    }
  }
  return false;
}

bool IsIntentValid(const apps::mojom::IntentPtr& intent) {
  // TODO(crbug.com/853604):Add more checks here to make this a general intent
  // validity check. Check if this is a share intent with no file or text.
  if (intent->action == kIntentActionSend ||
      intent->action == kIntentActionSendMultiple) {
    if (!intent->share_text.has_value() && !intent->file_urls.has_value()) {
      return false;
    }
  }
  return true;
}

}  // namespace apps_util
