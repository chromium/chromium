// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/webui/settings/settings_utils.h"

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/launch_application.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {
void ValidateFontFamily(PrefService* prefs, const char* family_pref_name) {
  // The native font settings dialog saved fonts by the font name, rather
  // than the family name.  This worked for the old dialog since
  // -[NSFont fontWithName:size] accepted a font or family name, but the
  // behavior was technically wrong.  Since we really need the family name for
  // the webui settings window, we will fix the saved preference if necessary.
  NSString* family_name =
      base::SysUTF8ToNSString(prefs->GetString(family_pref_name));
  NSFont* font = [NSFont fontWithName:family_name size:NSFont.systemFontSize];
  if (font &&
      [font.familyName caseInsensitiveCompare:family_name] != NSOrderedSame) {
    std::string new_family_name = base::SysNSStringToUTF8(font.familyName);
    prefs->SetString(family_pref_name, new_family_name);
  }
}
}  // namespace

namespace settings_utils {

void ShowNetworkProxySettings(content::WebContents* web_contents) {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kNetwork_Proxies);
}

void ShowManageSSLCertificates(content::WebContents* web_contents) {
  NSURL* keychain_app = [NSWorkspace.sharedWorkspace
      URLForApplicationWithBundleIdentifier:@"com.apple.keychainaccess"];
  base::mac::LaunchApplication(base::apple::NSURLToFilePath(keychain_app),
                               /*command_line_args=*/{}, /*url_specs=*/{},
                               /*options=*/{}, base::DoNothing());
}

void ValidateSavedFonts(PrefService* prefs) {
  ValidateFontFamily(prefs, prefs::kWebKitSerifFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitSansSerifFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitFixedFontFamily);
  ValidateFontFamily(prefs, prefs::kWebKitMathFontFamily);
}

}  // namespace settings_utils
