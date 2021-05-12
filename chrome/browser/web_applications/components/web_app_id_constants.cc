// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_id_constants.h"

namespace web_app {

// The URLs used to generate the app IDs MUST match the start_url field of the
// manifest served by the PWA.
// Please maintain the alphabetical order when adding new app IDs.

// TODO(crbug.com/1198418): Update when app URL is finalized.
const char kA4AppId[] = "ihckehilkadhemjaebeicgkdhdbfehcg";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://camera-app/views/main.html"))
const char kCameraAppId[] = "njfbnohfdkmbmnjapinfcopialeghnmh";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://canvas.apps.chrome/"))
const char kCanvasAppId[] = "ieailfmhaghpphfffooibmlghaeopach";

// Generated as: web_app::GenerateAppIdFromURL(GURL("chrome://diagnostics/"))
const char kDiagnosticsAppId[] = "keejpcfcpecjhmepmpcfgjemkmlicpam";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://mail.google.com/mail/?usp=installed_webapp"))
const char kGmailAppId[] = "fmgjjmmmlfnkbppncabfkddbjimcfncm";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://calendar.google.com/calendar/r"))
const char kGoogleCalendarAppId[] = "kjbdgfilnfhdoflbpgamdcdgpehopbep";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://docs.google.com/document/?usp=installed_webapp"))
const char kGoogleDocsAppId[] = "mpnpojknpmmopombnjdcgaaiekajbnjb";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://drive.google.com/?lfhs=2"))
const char kGoogleDriveAppId[] = "aghbiahbpaijignceidepookljebhfak";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://keep.google.com/?usp=installed_webapp"))
const char kGoogleKeepAppId[] = "eilembjdkfgodjkcjnpgpaenohkicgjd";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://www.google.com/maps?force=tt&source=ttpwa"))
const char kGoogleMapsAppId[] = "mnhkaebcjjhencmpkapnbdaogjamfbcj";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://news.google.com/?lfhs=2"))
const char kGoogleNewsAppId[] = "kfgapjallbhpciobgmlhlhokknljkgho";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://docs.google.com/spreadsheets/?usp=installed_webapp"))
const char kGoogleSheetsAppId[] = "fhihpiojkbmbpdjeoajapmgkhlnakfjf";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://docs.google.com/presentation/?usp=installed_webapp"))
const char kGoogleSlidesAppId[] = "kefjledonklijopmnomlcbpllchaibag";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://help-app/"))
const char kHelpAppId[] = "nbljnnecbjbmifnoehiemkgefbnpoeak";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://media-app/"))
const char kMediaAppId[] = "jhdjimmaggjajfjphpljagpgkidjilnj";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://messages.google.com/web/"))
const char kMessagesAppId[] = "hpfldicfbfomlpcikngkocigghgafkph";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://test-system-app/pwa.html"))
const char kMockSystemAppId[] = "maphiehpiinjgiaepbljmopkodkadcbh";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://os-settings/"))
const char kOsSettingsAppId[] = "odknhmnlageboeamepcngndbggdpaobj";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://books.google.com/ebooks/app"))
const char kPlayBooksAppId[] = "jglfhlbohpgcbefmhdmpancnijacbbji";

// Generated as:web_app::GenerateAppIdFromURL(GURL(
//      "chrome://print-management/"))
const char kPrintManagementAppId[] = "fglkccnmnaankjodgccmiodmlkpaiodc";

// Generated as: web_app::GenerateAppIdFromURL(GURL("chrome://scanning/"))
const char kScanningAppId[] = "cdkahakpgkdaoffdmfgnhgomkelkocfo";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://settings/"))
const char kSettingsAppId[] = "inogagmajamaleonmanpkpkkigmklfad";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://www.showtime.com/"))
const char kShowtimeAppId[] = "eoccpgmpiempcflglfokeengliildkag";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://stadia.google.com/?lfhs=2"))
const char kStadiaAppId[] = "pnkcfpnngfokcnnijgkllghjlhkailce";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://www.youtube.com/?feature=ytca"))
const char kYoutubeAppId[] = "agimnkijcaahngcdmfeangaknmldooml";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://music.youtube.com/?source=pwa"))
const char kYoutubeMusicAppId[] = "cinhimbnkkaeohfgghhklpknlkffjgod";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://tv.youtube.com/"))
const char kYoutubeTVAppId[] = "kiemjbkkegajmpbobdfngbmjccjhnofh";

}  // namespace web_app
