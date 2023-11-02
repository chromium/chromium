// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_ERROR_CODES_H_
#define CHROMECAST_BASE_ERROR_CODES_H_

namespace chromecast {

enum ErrorCode {
  NO_ERROR = 0,

  // web_content [1, 9999]
  ERROR_WEB_CONTENT_RENDER_VIEW_GONE = 1,
  ERROR_WEB_CONTENT_NAME_NOT_RESOLVED,
  ERROR_WEB_CONTENT_INTERNET_DISCONNECTED,

  // reboot [10000, 19999]
  // The following error codes do not reset the volume when the page is
  // launched.  Add codes that do not reset the volume before ERROR_REBOOT_NOW.
  ERROR_REBOOT_NOW = 10000,
  ERROR_REBOOT_FDR,
  ERROR_REBOOT_OTA,
  ERROR_REBOOT_IDLE,
  // Chromecast WebUI uses 19999 for END_OF_REBOOT_SECTION, so reserve it here.
  END_OF_REBOOT_SECTION = 19999,

  // misc [20000, 29999]
  ERROR_ABORTED = 20000,
  ERROR_LOST_PEER_CONNECTION = 20001,

  ERROR_UNKNOWN = 30000,
};

// Gets the error code for the first idle screen.
ErrorCode GetInitialErrorCode();

// Sets the error code for the first idle screen. Returns true if set
// successfully.
bool SetInitialErrorCode(ErrorCode initial_error_code);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_ERROR_CODES_H_
