// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_DRIVE_API_UTIL_H_
#define COMPONENTS_DRIVE_DRIVE_API_UTIL_H_

#include <string>

#include "components/drive/drive_export.h"

namespace base {
class AtomicFlag;
class FilePath;
}  // namespace base

namespace drive {
namespace util {

// Google Apps MIME types:
inline constexpr char kGoogleDocumentMimeType[] =
    "application/vnd.google-apps.document";
inline constexpr char kGoogleDrawingMimeType[] =
    "application/vnd.google-apps.drawing";
inline constexpr char kGooglePresentationMimeType[] =
    "application/vnd.google-apps.presentation";
inline constexpr char kGoogleSpreadsheetMimeType[] =
    "application/vnd.google-apps.spreadsheet";
inline constexpr char kGoogleTableMimeType[] =
    "application/vnd.google-apps.table";
inline constexpr char kGoogleFormMimeType[] =
    "application/vnd.google-apps.form";
inline constexpr char kGoogleMapMimeType[] = "application/vnd.google-apps.map";
inline constexpr char kGoogleSiteMimeType[] =
    "application/vnd.google-apps.site";
inline constexpr char kDriveFolderMimeType[] =
    "application/vnd.google-apps.folder";
inline constexpr char kEncryptedMimeType[] =
    "application/vnd.google-gsuite.encrypted";
inline constexpr char kEmailLayoutsMimeType[] =
    "application/vnd.google-apps.mail-layout";

// Escapes ' to \' in the |str|. This is designed to use for string value of
// search parameter on Drive API v2.
// See also: https://developers.google.com/drive/search-parameters
COMPONENTS_DRIVE_EXPORT
std::string EscapeQueryStringValue(const std::string& str);

// Parses the query, and builds a search query for Drive API v2.
// This only supports:
//   Regular query (e.g. dog => fullText contains 'dog')
//   Conjunctions
//     (e.g. dog cat => fullText contains 'dog' and fullText contains 'cat')
//   Exclusion query (e.g. -cat => not fullText contains 'cat').
//   Quoted query (e.g. "dog cat" => fullText contains 'dog cat').
// See also: https://developers.google.com/drive/search-parameters
COMPONENTS_DRIVE_EXPORT
std::string TranslateQuery(const std::string& original_query);

// If |resource_id| is in the old resource ID format used by WAPI, converts it
// into the new format.
COMPONENTS_DRIVE_EXPORT
std::string CanonicalizeResourceId(const std::string& resource_id);

// Returns the (base-16 encoded) MD5 digest of the file content at |file_path|,
// or an empty string if an error is found.
COMPONENTS_DRIVE_EXPORT
std::string GetMd5Digest(const base::FilePath& file_path,
                         const base::AtomicFlag* cancellation_flag);

// Returns true if the given mime type is corresponding to one of known hosted
// document types.
COMPONENTS_DRIVE_EXPORT
bool IsKnownHostedDocumentMimeType(const std::string& mime_type);

// Returns true if the given file path has an extension corresponding to one of
// hosted document types.
COMPONENTS_DRIVE_EXPORT
bool HasHostedDocumentExtension(const base::FilePath& path);

// Returns true if the given MIME type corresponds to an encrypted (Google Drive
// CSE) file.
COMPONENTS_DRIVE_EXPORT
bool IsEncryptedMimeType(const std::string& mime_type);

}  // namespace util
}  // namespace drive

#endif  // COMPONENTS_DRIVE_DRIVE_API_UTIL_H_
