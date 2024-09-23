// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace settings {

// Adds strings used by the <settings-ax-annotations-section> element.
void AddAxAnnotationsSectionStrings(content::WebUIDataSource* html_source);

// Adds strings used by the <settings-captions> element.
void AddCaptionSubpageStrings(content::WebUIDataSource* html_source);

// Adds strings used by the <settings-live-caption> element.
void AddLiveCaptionSectionStrings(content::WebUIDataSource* html_source);

#if BUILDFLAG(IS_CHROMEOS)
// Adds strings used by the <settings-password-prompt-dialog> element.
void AddPasswordPromptDialogStrings(content::WebUIDataSource* html_source);
#endif

// Adds strings used by both <settings-sync-page> and <os-settings-sync-subpage>
// elements.
void AddSharedSyncPageStrings(content::WebUIDataSource* html_source);

// Adds strings used by the <settings-secure-dns> element.
void AddSecureDnsStrings(content::WebUIDataSource* html_source);
}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_
