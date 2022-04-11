// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_id_constants.h"

#include "base/strings/string_piece.h"

namespace web_app {

// The URLs used to generate the app IDs MUST match the start_url field of the
// manifest served by the PWA.
// Please maintain the alphabetical order when adding new app IDs.

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://calculator.apps.chrome/"))
const char kCalculatorAppId[] = "oabkinaljpjeilageghcdlnekhphhphl";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://camera-app/views/main.html"))
const char kCameraAppId[] = "njfbnohfdkmbmnjapinfcopialeghnmh";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://canvas.apps.chrome/"))
const char kCanvasAppId[] = "ieailfmhaghpphfffooibmlghaeopach";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome-untrusted://crosh/"))
const char kCroshAppId[] = "cgfnfgkafmcdkdgilmojlnaadileaach";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://cursive.apps.chrome/"))
const char kCursiveAppId[] = "apignacaigpffemhdbhmnajajaccbckh";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt,
// GURL("chrome://diagnostics/"))
const char kDiagnosticsAppId[] = "keejpcfcpecjhmepmpcfgjemkmlicpam";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt,
// GURL("chrome://accessory-update/"))
const char kFirmwareUpdateAppId[] = "nedcdcceagjbkiaecmdbpafcmlhkiifa";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://mail.google.com/mail/?usp=installed_webapp"))
const char kGmailAppId[] = "fmgjjmmmlfnkbppncabfkddbjimcfncm";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://calendar.google.com/calendar/r"))
const char kGoogleCalendarAppId[] = "kjbdgfilnfhdoflbpgamdcdgpehopbep";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://mail.google.com/chat/"))
const char kGoogleChatAppId[] = "mdpkiolbdkhdjpekfbkbmhigcaggjagi";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://docs.google.com/document/?usp=installed_webapp"))
const char kGoogleDocsAppId[] = "mpnpojknpmmopombnjdcgaaiekajbnjb";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://drive.google.com/?lfhs=2"))
const char kGoogleDriveAppId[] = "aghbiahbpaijignceidepookljebhfak";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://keep.google.com/?usp=installed_webapp"))
const char kGoogleKeepAppId[] = "eilembjdkfgodjkcjnpgpaenohkicgjd";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://www.google.com/maps?force=tt&source=ttpwa"))
const char kGoogleMapsAppId[] = "mnhkaebcjjhencmpkapnbdaogjamfbcj";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://meet.google.com/landing?lfhs=2"))
const char kGoogleMeetAppId[] = "kjgfgldnnfoeklkmfkjfagphfepbbdan";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://play.google.com/store/movies?usp=installed_webapp"))
const char kGoogleMoviesAppId[] = "aiihaadhfoadjgjcegeomiajkajbjlcn";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://news.google.com/?lfhs=2"))
const char kGoogleNewsAppId[] = "kfgapjallbhpciobgmlhlhokknljkgho";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://docs.google.com/spreadsheets/?usp=installed_webapp"))
const char kGoogleSheetsAppId[] = "fhihpiojkbmbpdjeoajapmgkhlnakfjf";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://docs.google.com/presentation/?usp=installed_webapp"))
const char kGoogleSlidesAppId[] = "kefjledonklijopmnomlcbpllchaibag";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://help-app/"))
const char kHelpAppId[] = "nbljnnecbjbmifnoehiemkgefbnpoeak";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://media-app/"))
const char kMediaAppId[] = "jhdjimmaggjajfjphpljagpgkidjilnj";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://messages.google.com/web/"))
const char kMessagesAppId[] = "hpfldicfbfomlpcikngkocigghgafkph";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://test-system-app/pwa.html"))
const char kMockSystemAppId[] = "maphiehpiinjgiaepbljmopkodkadcbh";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://os-feedback/"))
const char kOsFeedbackAppId[] = "iffgohomcomlpmkfikfffagkkoojjffm";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://os-settings/"))
const char kOsSettingsAppId[] = "odknhmnlageboeamepcngndbggdpaobj";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://personalization/"))
const char kPersonalizationAppId[] = "glenkcidjgckcomnliblmkokolehpckn";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://books.google.com/ebooks/app"))
const char kPlayBooksAppId[] = "jglfhlbohpgcbefmhdmpancnijacbbji";

// Generated as:web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//      "chrome://print-management/"))
const char kPrintManagementAppId[] = "fglkccnmnaankjodgccmiodmlkpaiodc";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt,
// GURL("chrome://scanning/"))
const char kScanningAppId[] = "cdkahakpgkdaoffdmfgnhgomkelkocfo";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://settings/"))
const char kSettingsAppId[] = "inogagmajamaleonmanpkpkkigmklfad";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://shortcut-customization"))
const char kShortcutCustomizationAppId[] = "ihgeegogifolehadhdgelgcnbnmemikp";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://shimless-rma/"))
const char kShimlessRMAAppId[] = "ijolhdommgkkhpenofmpkkhlepahelcm";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://www.showtime.com/"))
const char kShowtimeAppId[] = "eoccpgmpiempcflglfokeengliildkag";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://stadia.google.com/?lfhs=2"))
const char kStadiaAppId[] = "pnkcfpnngfokcnnijgkllghjlhkailce";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://www.youtube.com/?feature=ytca"))
const char kYoutubeAppId[] = "agimnkijcaahngcdmfeangaknmldooml";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://music.youtube.com/?source=pwa"))
const char kYoutubeMusicAppId[] = "cinhimbnkkaeohfgghhklpknlkffjgod";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "https://tv.youtube.com/"))
const char kYoutubeTVAppId[] = "kiemjbkkegajmpbobdfngbmjccjhnofh";

#if !defined(OFFICIAL_BUILD)
// Generated as: web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(
//     "chrome://sample-system-web-app"))
const char kSampleSystemWebAppId[] = "jalmdcokfklmaoadompgacjlcomfckcf";
#endif  // !defined(OFFICIAL_BUILD)

bool IsSystemAppIdWithFileHandlers(base::StringPiece id) {
  return id == kMediaAppId;
}

}  // namespace web_app
