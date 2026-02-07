// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

#include "build/build_config.h"
#include "extensions/common/constants.h"

namespace extension_misc {

const char* const kBuiltInFirstPartyExtensionIds[] = {
    kCalculatorAppId,
    kCalendarAppId,
    kDataSaverExtensionId,
    kDocsOfflineExtensionId,
    kGoogleDriveAppId,
    kGmailAppId,
    kGoogleDocsAppId,
    kGoogleMapsAppId,
    kGooglePhotosAppId,
    kGooglePlayBooksAppId,
    kGooglePlayMoviesAppId,
    kGooglePlayMusicAppId,
    kGooglePlusAppId,
    kGoogleSheetsAppId,
    kGoogleSlidesAppId,
    kTextEditorAppId,
    kInAppPaymentsSupportAppId,
#if BUILDFLAG(IS_CHROMEOS)
    kAssessmentAssistantExtensionId,
    kAccessibilityCommonExtensionId,
    kSelectToSpeakExtensionId,
    kSwitchAccessExtensionId,
    kFilesManagerAppId,
    kFirstRunDialogId,
    kEspeakSpeechSynthesisExtensionId,
    kGoogleSpeechSynthesisExtensionId,
#endif  // BUILDFLAG(IS_CHROMEOS)
    kReadingModeGDocsHelperExtensionId,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    kTTSEngineExtensionId,
    kComponentUpdaterTTSEngineExtensionId,
#endif        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    nullptr,  // Null-terminated array.
};

}  // namespace extension_misc
