// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MANAGED_UI_H_
#define CHROME_BROWSER_UI_MANAGED_UI_H_

#include <optional>
#include <string>

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

class GURL;
class Profile;

namespace gfx {
struct VectorIcon;
}

// Returns true if a 'Managed by <...>' message should appear in
// Chrome's App Menu, and on the following chrome:// pages:
// - chrome://bookmarks
// - chrome://downloads
// - chrome://extensions
// - chrome://history
// - chrome://settings
//
// This applies to all forms of management (eg. both Enterprise and Parental
// controls), a suitable string will be returned by the methods below.
//
// N.B.: This is independent of Chrome OS's system tray message for enterprise
// users.
bool ShouldDisplayManagedUi(Profile* profile);

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)

// The URL which management surfaces should link to for more info.
//
// Returns an empty string if ShouldDisplayManagedUi(profile) is false.
GURL GetManagedUiUrl(Profile* profile);

// The icon to use in the Managed UI.
const gfx::VectorIcon& GetManagedUiIcon(Profile* profile);

// The label for the App Menu item for Managed UI.
//
// Must only be called if ShouldDisplayManagedUi(profile) is true.
std::u16string GetManagedUiMenuItemLabel(Profile* profile);

// The tooltip for the App Menu item for Managed UI.
//
// Must only be called if ShouldDisplayManagedUi(profile) is true.
std::u16string GetManagedUiMenuItemTooltip(Profile* profile);

// An icon name/label recognized by <iron-icon> for the WebUI footnote for
// Managed UI indicating that the browser is managed.
//
// Returns an empty string if ShouldDisplayManagedUi(profile) is false.
std::string GetManagedUiWebUIIcon(Profile* profile);

// The label for the WebUI footnote for Managed UI indicating that the browser
// is managed. These strings contain HTML for an <a> element.
//
// Returns an empty string if ShouldDisplayManagedUi(profile) is false.
std::u16string GetManagedUiWebUILabel(Profile* profile);

// The label for the string describing whether the browser is managed or not, in
// the chrome://settings/help page.
std::u16string GetDeviceManagedUiHelpLabel(Profile* profile);
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(IS_CHROMEOS)
// The label for the WebUI footnote for Managed UI indicating that the device
// is mananged. These strings contain HTML for an <a> element.
std::u16string GetDeviceManagedUiWebUILabel();
#else
std::u16string GetManagementPageSubtitle(Profile* profile);
std::u16string GetManagementBubbleTitle(Profile* profile);
#endif

// Returns trus if the profile and browser are managed and both entities are
// known and different.
bool AreProfileAndBrowserManagedBySameEntity(Profile* profile);

#endif  // CHROME_BROWSER_UI_MANAGED_UI_H_
