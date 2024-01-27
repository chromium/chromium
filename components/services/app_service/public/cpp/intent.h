// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/safe_base_name.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "url/gurl.h"

namespace apps {

// Metadata for a single file shared through an intent.
struct IntentFile {
  explicit IntentFile(const GURL& url);
  IntentFile(const IntentFile&) = delete;
  IntentFile& operator=(const IntentFile&) = delete;
  ~IntentFile();

  bool operator==(const IntentFile& other) const;
  bool operator!=(const IntentFile& other) const;

  std::unique_ptr<IntentFile> Clone() const;

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
  std::optional<std::string> mime_type;
  // Human readable file name, including extension, and not allow absolute paths
  // or references to parent directories.
  std::optional<base::SafeBaseName> file_name;
  // File size in bytes.
  uint64_t file_size = 0;
  // Whether this is a directory or not.
  std::optional<bool> is_directory;
  // Source URL the file was downloaded from. Used to check Data Leak Prevention
  // (DLP) restrictions when resolving the intent.
  std::optional<std::string> dlp_source_url;
};

using IntentFilePtr = std::unique_ptr<IntentFile>;

// Action and resource handling request. This should be kept in sync with
// ConvertIntentToValue and ConvertValueToIntent in
// components/services/app_service/public/cpp/intent_util.*
struct Intent {
  // Factory methods for more complicated Intents are available in
  // intent_util.h.
  explicit Intent(const std::string& action);
  explicit Intent(const std::string& action, const GURL& url);
  explicit Intent(const std::string& action, std::vector<IntentFilePtr> files);

  Intent(const Intent&) = delete;
  Intent& operator=(const Intent&) = delete;
  ~Intent();

  bool operator==(const Intent& other) const;
  bool operator!=(const Intent& other) const;

  std::unique_ptr<Intent> Clone() const;

  // Gets the field that need to be checked/matched based on `condition_type`.
  std::optional<std::string> GetIntentConditionValueByType(
      ConditionType condition_type);

  // Returns true if matches the authority `condition`, otherwise, returns
  // false.
  bool MatchAuthorityCondition(const ConditionPtr& condition);

  // Returns true if matches the file `condition`, otherwise, returns false.
  bool MatchFileCondition(const ConditionPtr& condition);

  // Returns true if matches with any of the values in `condition`.
  bool MatchCondition(const ConditionPtr& condition);

  // Returns true if matches all existing conditions in the filter.
  bool MatchFilter(const IntentFilterPtr& filter);

  // Returns true if `intent` corresponds to a share intent.
  bool IsShareIntent();

  // Check if the intent only mean to share to Google Drive.
  bool OnlyShareToDrive();

  // Check the if the intent is valid, e.g. action matches content.
  bool IsIntentValid();

  // Intent action. e.g. view, send.
  std::string action;
  // The URL of the intent. e.g. https://www.google.com/.
  std::optional<GURL> url;

  // MIME type. e.g. text/plain, image/*.
  std::optional<std::string> mime_type;

  // The files to share.
  std::vector<IntentFilePtr> files;
  // The activity for the app to launch.
  std::optional<std::string> activity_name;

  // The Drive share URL, this is only filled if the intent contains a file
  // from Google Drive.
  std::optional<GURL> drive_share_url;
  // Text to share. e.g. Share link to other app.
  std::optional<std::string> share_text;
  // Title for the share.
  std::optional<std::string> share_title;
  // Start type.
  std::optional<std::string> start_type;
  std::vector<std::string> categories;
  // URI
  std::optional<std::string> data;
  // Whether or not the user saw the UI.
  std::optional<bool> ui_bypassed;
  // Optional string extras.
  base::flat_map<std::string, std::string> extras;
};

using IntentPtr = std::unique_ptr<Intent>;

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_H_
