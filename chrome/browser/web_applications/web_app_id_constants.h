// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_CONSTANTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_CONSTANTS_H_

#include "base/strings/string_piece_forward.h"

namespace web_app {

extern const char kCalculatorAppId[];
extern const char kCameraAppId[];
extern const char kCanvasAppId[];
extern const char kCloudGamingPartnerPlatform[];
extern const char kCroshAppId[];
extern const char kCursiveAppId[];
extern const char kDiagnosticsAppId[];
extern const char kFirmwareUpdateAppId[];
extern const char kGmailAppId[];
extern const char kGoogleCalendarAppId[];
extern const char kGoogleChatAppId[];
extern const char kGoogleDocsAppId[];
extern const char kGoogleDriveAppId[];
extern const char kGoogleKeepAppId[];
extern const char kGoogleMapsAppId[];
extern const char kGoogleMeetAppId[];
extern const char kGoogleMoviesAppId[];
extern const char kGoogleNewsAppId[];
extern const char kGoogleSheetsAppId[];
extern const char kGoogleSlidesAppId[];
extern const char kHelpAppId[];
extern const char kMediaAppId[];
extern const char kMessagesAppId[];
extern const char kMessagesDogfoodDeprecatedAppId[];
extern const char kMockSystemAppId[];
extern const char kOsFeedbackAppId[];
extern const char kOsSettingsAppId[];
extern const char kPersonalizationAppId[];
extern const char kPlayBooksAppId[];
extern const char kPrintManagementAppId[];
extern const char kScanningAppId[];
extern const char kSettingsAppId[];
extern const char kShortcutCustomizationAppId[];
extern const char kShimlessRMAAppId[];
extern const char kShowtimeAppId[];
extern const char kStadiaAppId[];
extern const char kYoutubeAppId[];
extern const char kYoutubeMusicAppId[];
extern const char kYoutubeTVAppId[];

#if !defined(OFFICIAL_BUILD)
extern const char kSampleSystemWebAppId[];
#endif  // !defined(OFFICIAL_BUILD)

bool IsSystemAppIdWithFileHandlers(base::StringPiece id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_CONSTANTS_H_
