// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_

// Utility functions for App Service intent handling.

#include <optional>
#include <string>
#include <string_view>

#include "base/values.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace apps_util {

extern const char kIntentActionMain[];
extern const char kIntentActionView[];
extern const char kIntentActionSend[];
extern const char kIntentActionSendMultiple[];
extern const char kIntentActionCreateNote[];
extern const char kIntentActionStartOnLockScreen[];
// A request to edit a file in an app. Must include an attached file.
extern const char kIntentActionEdit[];
extern const char kIntentActionPotentialFileHandler[];

// App ID value which can be used as a Preferred App to denote that the browser
// will open the link, and that we should not prompt the user about it.
extern const char kUseBrowserForLink[];

// Activity name for GuestOS intent filters. TODO(crbug.com/40233967): Remove
// when default file handling preferences for Files App are migrated.
extern const char kGuestOsActivityName[];

struct SharedText {
  std::string text;
  GURL url;
};

// Creates an intent for sharing |filesystem_urls|. |filesystem_urls| must be
// co-indexed with |mime_types|.
apps::IntentPtr MakeShareIntent(const std::vector<GURL>& filesystem_urls,
                                const std::vector<std::string>& mime_types);

// Creates an intent for sharing |filesystem_urls|, along with |text| and a
// |title|. |filesystem_urls| must be co-indexed with |mime_types|.
apps::IntentPtr MakeShareIntent(const std::vector<GURL>& filesystem_urls,
                                const std::vector<std::string>& mime_types,
                                const std::string& text,
                                const std::string& title);

// Creates an intent for sharing `filesystem_url`, `mime_type` and
// `drive_share_url` for a Google Drive file.
apps::IntentPtr MakeShareIntent(const GURL& filesystem_url,
                                const std::string& mime_type,
                                const GURL& drive_share_url,
                                bool is_directory);

// Creates an intent for sharing |text|, with |title|.
apps::IntentPtr MakeShareIntent(const std::string& text,
                                const std::string& title);

// Creates an intent for sharing |filesystem_urls|, with |dlpSourceUrls|.
apps::IntentPtr MakeShareIntent(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types,
    const std::vector<std::string>& dlp_source_urls);

// Create an edit intent for the file with a given |filesystem_url| and
// |mime_type|.
apps::IntentPtr MakeEditIntent(const GURL& filesystem_url,
                               const std::string& mime_type);

// Create an intent struct from activity and start type.
apps::IntentPtr MakeIntentForActivity(const std::string& activity,
                                      const std::string& start_type,
                                      const std::string& category);

// Create an intent struct for a Create Note action.
apps::IntentPtr CreateCreateNoteIntent();

// Create an intent struct for a "Start On Lock Screen" action.
apps::IntentPtr CreateStartOnLockScreenIntent();

// Return true if |value| matches with the |condition_value|, based on the
// pattern match type in the |condition_value|.
bool ConditionValueMatches(std::string_view value,
                           const apps::ConditionValuePtr& condition_value);

bool PatternMatchValue(std::string_view test_value,
                       apps::PatternMatchType match_type,
                       std::string_view match_value);

bool IsGenericFileHandler(const apps::IntentPtr& intent,
                          const apps::IntentFilterPtr& filter);

// Return true if |value| matches |pattern| with simple glob syntax.
// In this syntax, you can use the '*' character to match against zero or
// more occurrences of the character immediately before. If the character
// before it is '.' it will match any character. The character '\' can be
// used as an escape. This essentially provides only the '*' wildcard part
// of a normal regexp.
// This function is transcribed from android's PatternMatcher#matchPattern.
// See
// https://android.googlesource.com/platform/frameworks/base.git/+/e93165456c3c28278f275566bd90bfbcf1a0e5f7/core/java/android/os/PatternMatcher.java#186
bool MatchGlob(std::string_view value, std::string_view pattern);

// TODO(crbug.com/40134747): Handle file path with extension with mime type.
// Unlike Android mime type matching logic, if the intent mime type has *, it
// can only match with *, not anything. The reason for this is the way we find
// the common mime type for multiple files. It uses * to represent more than one
// types in the list, which will cause an issue if we treat that as we want to
// match with any filter. e.g. If we select a .zip, .jep and a .txt, the common
// mime type will be */*, with Android matching logic, it will match with filter
// that has mime type video, which is not what we expected.
bool MimeTypeMatched(std::string_view intent_mime_type,
                     std::string_view filter_mime_type);

bool ExtensionMatched(const std::string& file_name,
                      const std::string& filter_extension);

// Converts |intent| to base::Value, e.g.:
// {
//    "action": "xx",
//    "url": "abc.com",
//    "mime_type": "text/plain",
//    "file_urls": "/abc, /a",
//    "activity_name": "yy",
//    "drive_share_url": "aa.com",
//    "share_text": "text",
//    "share_title": "title",
// }
base::Value ConvertIntentToValue(const apps::IntentPtr& intent);

// Converts base::Value to Intent. Returns nullptr for invalid base::Values.
apps::IntentPtr ConvertValueToIntent(base::Value&& value);
apps::IntentPtr ConvertDictToIntent(const base::Value::Dict& dict);

// Calculates the least general mime type that matches all of the given ones.
// E.g., for ["image/jpeg", "image/png"] it will be "image/*". ["text/html",
// "text/html"] will return "text/html", and ["text/html", "image/jpeg"]
// becomes the fully wildcard pattern.
std::string CalculateCommonMimeType(const std::vector<std::string>& mime_types);

// Extracts the text from |share_text| to populate the SharedText struct. If
// |SharedText.url| is populated, the value will always be a valid parsed URL.
// The |share_text| passed in here should be the share_text field from Intent.
//
// Testing covered by share_target_utils_unittest.cc as this function was
// migrated out from web_app::ShareTargetUtils.
SharedText ExtractSharedText(const std::string& share_text);

// A view object onto a host and optional port string, represents the same thing
// as arc::IntentFilter::AuthorityEntry with an emphasis on string
// encoding/decoding for use with ConditionValue's std::string value.
// The underlying strings must be kept alive while the AuthorityView is around.
struct AuthorityView {
  const std::string_view host;
  const std::optional<std::string_view> port;

  // Stringifies the effective port of `url` if there is one. Not all URL
  // schemes have ports.
  static std::optional<std::string> PortToString(const GURL& url);
  static std::optional<std::string> PortToString(const url::Origin& url);

  // Decodes strings of the form:
  // "www.example.com:1234" into {.host="www.example.com", .port="1234"}
  // or "www.example.com" into {.host="www.example.com", .port=nullopt}
  static AuthorityView Decode(std::string_view);

  // Delegates to Encode().
  static std::string Encode(const GURL& url);
  static std::string Encode(const url::Origin& origin);

  // Encodes into the form:
  // "www.example.com:1234" if port is set
  // or "www.example.com" if port is unset.
  // Note that the default scheme port will be used even if not explicitly
  // specified e.g.: Encode(GURL("https://www.foo.com")) == "www.foo.com:443"
  std::string Encode();
};

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_
