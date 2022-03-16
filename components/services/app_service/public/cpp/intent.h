// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_H_

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/safe_base_name.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace apps {

// Metadata for a single file shared through an intent.
struct IntentFile {
  explicit IntentFile(const GURL& url);
  IntentFile(const IntentFile&) = delete;
  IntentFile& operator=(const IntentFile&) = delete;
  ~IntentFile();

  // Returns true if matches `condition_value`, otherwise, returns false.
  bool MatchConditionValue(const ConditionValuePtr& condition_value);

  // Returns true if matches any condition in `condition_values`, otherwise,
  // returns false.
  bool MatchAnyConditionValue(
      const std::vector<ConditionValuePtr>& condition_values);

  // The URL of the file to share. Normally has the filesystem: scheme, but
  // could be externalfile: or a different scheme, depending on the source.
  GURL url;

  // The following optional fields can be provided to supply additional metadata
  // information in cases where fetching the metadata through the file would be
  // difficult or expensive.

  // File MIME type.
  absl::optional<std::string> mime_type;
  // Human readable file name, including extension, and not allow absolute paths
  // or references to parent directories.
  absl::optional<base::SafeBaseName> file_name;
  // File size in bytes.
  uint64_t file_size = 0;
  // Whether this is a directory or not.
  absl::optional<bool> is_directory;
};

using IntentFilePtr = std::unique_ptr<IntentFile>;

// Action and resource handling request. This should be kept in sync with
// ConvertIntentToValue and ConvertValueToIntent in
// components/services/app_service/public/cpp/intent_util.*
struct Intent {
  explicit Intent(const std::string& action);
  explicit Intent(const GURL& url);

  // Creates an intent for sharing `filesystem_urls`. `filesystem_urls` must be
  // co-indexed with `mime_types`.
  Intent(const std::vector<GURL>& filesystem_urls,
         const std::vector<std::string>& mime_types);

  // Creates an intent with the list of `files`.
  explicit Intent(std::vector<IntentFilePtr> files);

  // Creates an intent for sharing `filesystem_urls`, along with `text` content
  // and `title`. `filesystem_urls` must be co-indexed with  mime_types.
  Intent(const std::vector<GURL>& filesystem_urls,
         const std::vector<std::string>& mime_types,
         const std::string& text,
         const std::string& title);

  // Creates an intent for sharing `text`, with `title`.
  Intent(const std::string& text, const std::string& title);

  Intent(const Intent&) = delete;
  Intent& operator=(const Intent&) = delete;
  ~Intent();

  // Gets the field that need to be checked/matched based on `condition_type`.
  absl::optional<std::string> GetIntentConditionValueByType(
      ConditionType condition_type);

  // Returns true if matches the file `condition`, otherwise, returns false.
  bool MatchFileCondition(const ConditionPtr& condition);

  // Returns true if matches with any of the values in `condition`.
  bool MatchCondition(const ConditionPtr& condition);

  // Returns true if matches all existing conditions in the filter.
  bool MatchFilter(const IntentFilterPtr& filter);

  // Intent action. e.g. view, send.
  std::string action;
  // The URL of the intent. e.g. https://www.google.com/.
  absl::optional<GURL> url;

  // MIME type. e.g. text/plain, image/*.
  absl::optional<std::string> mime_type;

  // The files to share.
  std::vector<IntentFilePtr> files;
  // The activity for the app to launch.
  absl::optional<std::string> activity_name;

  // The Drive share URL, this is only filled if the intent contains a file
  // from Google Drive.
  absl::optional<GURL> drive_share_url;
  // Text to share. e.g. Share link to other app.
  absl::optional<std::string> share_text;
  // Title for the share.
  absl::optional<std::string> share_title;
  // Start type.
  absl::optional<std::string> start_type;
  std::vector<std::string> categories;
  // URI
  absl::optional<std::string> data;
  // Whether or not the user saw the UI.
  absl::optional<bool> ui_bypassed;
  // Optional string extras.
  base::flat_map<std::string, std::string> extras;
};

using IntentPtr = std::unique_ptr<Intent>;

// TODO(crbug.com/1253250): Remove these functions after migrating to non-mojo
// AppService.
IntentFilePtr ConvertMojomIntentFileToIntentFile(
    const apps::mojom::IntentFilePtr& mojom_intent_file);

apps::mojom::IntentFilePtr ConvertIntentFileToMojomIntentFile(
    const IntentFilePtr& intent_file);

IntentPtr ConvertMojomIntentToIntent(
    const apps::mojom::IntentPtr& mojom_intent);

apps::mojom::IntentPtr ConvertIntentToMojomIntent(const IntentPtr& intent);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_H_
