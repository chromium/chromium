// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_REQUEST_HEADER_OFFLINE_PAGE_HEADER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_REQUEST_HEADER_OFFLINE_PAGE_HEADER_H_

#include <string>

#include "url/gurl.h"

namespace offline_pages {

// Header that defines the custom behavior to load offline pages. Its value is a
// comma/space separated name-value pair.
extern const char kOfflinePageHeader[];

// The name used in name-value pair of kOfflinePageHeader to tell if the offline
// info in this header should be persisted across session restore.
extern const char kOfflinePageHeaderPersistKey[];

// The name used in name-value pair of kOfflinePageHeader to denote the reason
// for loading offline page.
extern const char kOfflinePageHeaderReasonKey[];
// Possible values in name-value pair that denote the reason for loading offline
// page.
// The offline page should be loaded even when the network is connected. This is
// because the live version failed to load due to certain net error.
extern const char kOfflinePageHeaderReasonValueDueToNetError[];
// The offline page should be loaded because the user clicks the downloaded page
// in Downloads home.
extern const char kOfflinePageHeaderReasonValueFromDownload[];
// This only happens after the offline page is loaded due to above reason and
// then the user reload current page. The network condition should be checked
// this time to decide if a live version should be tried again.
extern const char kOfflinePageHeaderReasonValueReload[];
// The offline page should be loaded because the user clicks the notification
// about a downloaded page.
extern const char kOfflinePageHeaderReasonValueFromNotification[];
// The offline page may be loaded when a file URL intent to view MHTML file is
// received by Chrome (Android only).
extern const char kOfflinePageHeaderReasonValueFromFileUrlIntent[];
// The offline page may be loaded when a content URL intent to view MHTML
// content is received by Chrome (Android only).
extern const char kOfflinePageHeaderReasonValueFromContentUrlIntent[];
// The offline page should be loaded because the user clicks the notification
// about a downloaded page.
extern const char kOfflinePageHeaderReasonValueFromNotification[];
// The offline page should be loaded because the user clicks Open link in
// the progress bar.
extern const char kOfflinePageHeadeReasonValueFromProgressBar[];
// The offline page should be loaded because the user clicks an offlined
// suggested item.
extern const char kOfflinePageHeadeReasonValueFromSuggestion[];
// The offline page should be loaded because the user clicks on a offline
// content suggestion on the net error page.
extern const char kOfflinePageHeaderNetErrorSuggestion[];

// The name used in name-value pair of kOfflinePageHeader to denote the offline
// ID of the offline page to load.
extern const char kOfflinePageHeaderIDKey[];

// The name used in name-value pair of kOfflinePageHeader to provide the source
// that may trigger loading the offline page.
extern const char kOfflinePageHeaderIntentUrlKey[];

// Used to parse the extra request header string that defines offline page
// loading behaviors.
struct OfflinePageHeader {
 public:
  enum class Reason {
    NONE,
    NET_ERROR,
    DOWNLOAD,
    RELOAD,
    NOTIFICATION,
    FILE_URL_INTENT,
    CONTENT_URL_INTENT,
    PROGRESS_BAR,
    SUGGESTION,
    NET_ERROR_SUGGESTION,
  };

  OfflinePageHeader();

  // Constructed from a request header value string.
  // The struct members will be cleared if the parsing failed.
  explicit OfflinePageHeader(const std::string& header_value);

  ~OfflinePageHeader();

  // Returns the full header string, including both key and value, that could be
  // passed to set extra request header.
  std::string GetCompleteHeaderString() const;

  // Returns the key string used for extra request header. Empty string is
  // returned if this struct is empty.
  std::string GetHeaderKeyString() const;

  // Returns the valye string used for extra request header. Empty string is
  // returned if this struct is empty.
  std::string GetHeaderValueString() const;

  void Clear();

  // Set if failed to parse a request header value string. For testing only.
  bool did_fail_parsing_for_test;

  // Flag to indicate if the header should be persisted across session restore.
  bool need_to_persist;

  // Describes the reason to load offline page.
  Reason reason;

  // The offline ID of the page to load.
  std::string id;

  // The URL of the intent that may trigger loading the offline page (Android
  // only).
  GURL intent_url;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_REQUEST_HEADER_OFFLINE_PAGE_HEADER_H_
