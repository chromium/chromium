// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MANAGED_UI_H_
#define CHROME_BROWSER_UI_MANAGED_UI_H_

#include <optional>
#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class GURL;
class Profile;

namespace gfx {
struct VectorIcon;
}

namespace chrome {

class ScopedDeviceManagerForTesting {
 public:
  explicit ScopedDeviceManagerForTesting(const char* manager);
  ~ScopedDeviceManagerForTesting();

 private:
  const char* previous_manager_ = nullptr;
};

// Returns the enterprise domain of `profile` if one was found.
// This function will try to get the hosted domain and fallback on the domain
// of the email of the signed in account.
std::optional<std::string> GetEnterpriseAccountDomain(const Profile& profile);

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

#if !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The label for the WebUI footnote for Managed UI indicating that the device
// is mananged. These strings contain HTML for an <a> element.
std::u16string GetDeviceManagedUiWebUILabel();
#else
// The subtitle for the management page.
std::u16string GetManagementPageSubtitle(Profile* profile);
#endif

#if !BUILDFLAG(IS_CHROMEOS)
std::u16string GetManagementBubbleTitle(Profile* profile);
#endif

// Returns trus if the profile and browser are managed and both entities are
// known and different.
bool AreProfileAndBrowserManagedBySameEntity(Profile* profile);

// Returns nullopt if the device is not managed, the UTF8-encoded string
// representation of the manager identity if available and an empty string if
// the device is managed but the manager is not known or if the policy store
// hasn't been loaded yet.
std::optional<std::string> GetDeviceManagerIdentity();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns the UTF8-encoded string representation of the the entity that manages
// the current session or nullopt if unmanaged. Returns the same result as
// `GetAccountManagerIdentity(primary_profile)` where `primary_profile` is the
// initial profile in the session. This concept only makes sense on lacros where
//  - session manager can be different from account manager for a profile in
//    this session, and also
//  - session manager can be different from device manager.
std::optional<std::string> GetSessionManagerIdentity();
#endif

// Returns the UTF8-encoded string representation of the the entity that manages
// `profile` or nullopt if unmanaged. For standard dasher domains, this will be
// a domain name (ie foo.com). For FlexOrgs, this will be the email address of
// the admin of the FlexOrg (ie user@foo.com). If DMServer does not provide this
// information, this function defaults to the domain of the account.
// TODO(crbug.com/40130449): Refactor localization hints for all strings that
// depend on this function.
std::optional<std::string> GetAccountManagerIdentity(Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_MANAGED_UI_H_
