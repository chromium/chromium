// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    kAccessibilityCommonExtensionId,
    kSelectToSpeakExtensionId,
    kSwitchAccessExtensionId,
    kFilesManagerAppId,
    kFirstRunDialogId,
    kEspeakSpeechSynthesisExtensionId,
    kGoogleSpeechSynthesisExtensionId,
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    kEmbeddedA11yHelperExtensionId,
    kChromeVoxHelperExtensionId,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    kReadingModeGDocsHelperExtensionId,
#endif        // !BUILDFLAG(IS_CHROMEOS_LACROS)
    nullptr,  // Null-terminated array.
};

}  // namespace extension_misc
