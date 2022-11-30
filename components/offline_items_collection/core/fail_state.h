// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_FAIL_STATE_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_FAIL_STATE_H_

#include <iosfwd>

namespace offline_items_collection {

// Warning: These enumeration values are saved to a database, enumeration values
// should not be changed.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_items_collection
enum class FailState {
  // Enum for reason OfflineItem failed to download.
  NO_FAILURE,       // Download did not fail.
  CANNOT_DOWNLOAD,  // Download cannot be downloaded. Deprecated, use detailed
                    // states below.
  NETWORK_INSTABILITY,  // Download failed due to poor or unstable network
                        // connection. Deprecated, use detailed states below.
  // Generic file operation failure.
  FILE_FAILED,

  // The file cannot be accessed due to security restrictions.
  FILE_ACCESS_DENIED,

  // There is not enough room on the drive.
  FILE_NO_SPACE,

  // The directory or file name is too long.
  FILE_NAME_TOO_LONG,

  // The file is too large for the file system to handle.
  FILE_TOO_LARGE,

  // The file contains a virus.
  FILE_VIRUS_INFECTED,

  // The file was in use.
  // Too many files are opened at once.
  // We have run out of memory.
  FILE_TRANSIENT_ERROR,

  // The file was blocked due to local policy.
  FILE_BLOCKED,

  // An attempt to check the safety of the download failed due to unexpected
  // reasons. See http://crbug.com/153212.
  FILE_SECURITY_CHECK_FAILED,

  // An attempt was made to seek past the end of a file in opening
  // a file (as part of resuming a previously interrupted download).
  FILE_TOO_SHORT,

  // The partial file didn't match the expected hash.
  FILE_HASH_MISMATCH,

  // The source and the target of the download were the same.
  FILE_SAME_AS_SOURCE,

  // Network errors.

  // Generic network failure.
  NETWORK_FAILED,

  // The network operation timed out.
  NETWORK_TIMEOUT,

  // The network connection has been lost.
  NETWORK_DISCONNECTED,

  // The server has gone down.
  NETWORK_SERVER_DOWN,

  // The network request was invalid. This may be due to the original URL or a
  // redirected URL:
  // - Having an unsupported scheme.
  // - Being an invalid URL.
  // - Being disallowed by policy.
  NETWORK_INVALID_REQUEST,

  // Server responses.

  // The server indicates that the operation has failed (generic).
  SERVER_FAILED,

  // The server does not support range requests.
  // Internal use only:  must restart from the beginning.
  SERVER_NO_RANGE,

  // The server does not have the requested data.
  SERVER_BAD_CONTENT,

  // Server didn't authorize access to resource.
  SERVER_UNAUTHORIZED,

  // Server certificate problem.
  SERVER_CERT_PROBLEM,

  // Server access forbidden.
  SERVER_FORBIDDEN,

  // Unexpected server response. This might indicate that the responding server
  // may not be the intended server.
  SERVER_UNREACHABLE,

  // The server sent fewer bytes than the content-length header. It may indicate
  // that the connection was closed prematurely, or the Content-Length header
  // was
  // invalid. The download is only interrupted if strong validators are present.
  // Otherwise, it is treated as finished.
  SERVER_CONTENT_LENGTH_MISMATCH,

  // An unexpected cross-origin redirect happened.
  SERVER_CROSS_ORIGIN_REDIRECT,

  // User input.

  // The user canceled the download.
  USER_CANCELED,

  // The user shut down the browser.
  USER_SHUTDOWN,

  // Crash.

  // The browser crashed.
  CRASH,
};

bool ToFailState(int value, FailState* fail_state);

// Implemented for testing only. See test_support/offline_item_test_support.cc.
std::ostream& operator<<(std::ostream& os, FailState state);

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_FAIL_STATE_H_
