// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_GCAPI_H_
#define CHROME_INSTALLER_GCAPI_GCAPI_H_

#include <windows.h>

// Error conditions for GoogleChromeCompatibilityCheck().
#define GCCC_ERROR_USERLEVELALREADYPRESENT (1 << 0)
#define GCCC_ERROR_SYSTEMLEVELALREADYPRESENT (1 << 1)
#define GCCC_ERROR_ACCESSDENIED (1 << 2)
#define GCCC_ERROR_OSNOTSUPPORTED (1 << 3)
#define GCCC_ERROR_ALREADYOFFERED (1 << 4)
#define GCCC_ERROR_INTEGRITYLEVEL (1 << 5)

// Error conditions for CanReactivateChrome().
#define REACTIVATE_ERROR_NOTINSTALLED (1 << 0)
#define REACTIVATE_ERROR_NOTDORMANT (1 << 1)
#define REACTIVATE_ERROR_ALREADY_REACTIVATED (1 << 2)
#define REACTIVATE_ERROR_INVALID_INPUT (1 << 3)
#define REACTIVATE_ERROR_REACTIVATION_FAILED (1 << 4)

// Error conditions for CanOfferRelaunch().
#define RELAUNCH_ERROR_NOTINSTALLED (1 << 0)
#define RELAUNCH_ERROR_INVALID_PARTNER (1 << 1)
#define RELAUNCH_ERROR_PINGS_SENT (1 << 2)
#define RELAUNCH_ERROR_NOTDORMANT (1 << 3)
#define RELAUNCH_ERROR_ALREADY_RELAUNCHED (1 << 4)
#define RELAUNCH_ERROR_INVALID_INPUT (1 << 5)
#define RELAUNCH_ERROR_RELAUNCH_FAILED (1 << 6)

// Flags to indicate how GCAPI is invoked
#define GCAPI_INVOKED_STANDARD_SHELL (1 << 0)
#define GCAPI_INVOKED_UAC_ELEVATION (1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

// The minimum number of days an installation can be dormant before reactivation
// may be offered.
const int kReactivationMinDaysDormant = 50;

// The minimum number of days an installation can be dormant before a relaunch
// may be offered.
const int kRelaunchMinDaysDormant = 30;

// This function returns TRUE if Google Chrome should be offered.
// If the return is FALSE, the |reasons| DWORD explains why.  If you don't care
// for the reason, you can pass nullptr for |reasons|.
// |set_flag| indicates whether a flag should be set indicating that Chrome was
// offered within the last six months; if passed FALSE, this method will not
// set the flag even if Chrome can be offered.  If passed TRUE, this method
// will set the flag only if Chrome can be offered.
// |shell_mode| should be set to one of GCAPI_INVOKED_STANDARD_SHELL or
// GCAPI_INVOKED_UAC_ELEVATION depending on whether this method is invoked
// from an elevated or non-elevated process.
BOOL __stdcall GoogleChromeCompatibilityCheck(BOOL set_flag,
                                              int shell_mode,
                                              DWORD* reasons);

// This function launches Google Chrome after a successful install. Make
// sure COM library is NOT initialized before you call this function (so if
// you called CoInitialize, call CoUninitialize before calling this function).
BOOL __stdcall LaunchGoogleChrome();

// This function launches Google Chrome after a successful install, ensuring
// that any windows that it makes are shunted to the background. Make sure COM
// library is NOT initialized before you call this function (so if you called
// CoInitialize, call CoUninitialize before calling this function).
BOOL __stdcall LaunchGoogleChromeInBackground();

// This function launches Google Chrome after a successful install at the given
// x,y coordinates with size height,length. Pass -1 for x and y to avoid moving
// the window. Pass -1 for width and height to avoid resizing the window. Set
// in_background to true to move Google Chrome behind all other windows or false
// to have it appear at the default z-order. Make sure that COM is NOT
// initialized before you call this function (so if you called CoInitialize,
// call CoUninitialize before calling this function). This call is synchronous,
// meaning it waits for Chrome to launch and appear to resize it before
// returning.
BOOL __stdcall LaunchGoogleChromeWithDimensions(int x,
                                                int y,
                                                int width,
                                                int height,
                                                bool in_background);

// This function returns the number of days since Google Chrome was last run by
// the current user. If both user-level and machine-wide installations are
// present on the system, it will return the lowest last-run-days count of
// the two.
// Returns -1 if Chrome is not installed, the last run date is in the future,
// or we are otherwise unable to determine how long since Chrome was last
// launched.
int __stdcall GoogleChromeDaysSinceLastRun();

// Returns true if a vendor with the specified |brand_code| may offer
// reactivation at this time. Returns false if the vendor may not offer
// reactivation at this time, and places one of the REACTIVATE_ERROR_XXX values
// in |error_code| if |error_code| is non-null.
// |shell_mode| should be set to one of GCAPI_INVOKED_STANDARD_SHELL or
// GCAPI_INVOKED_UAC_ELEVATION depending on whether this method is invoked
// from an elevated or non-elevated process.
BOOL __stdcall CanOfferReactivation(const wchar_t* brand_code,
                                    int shell_mode,
                                    DWORD* error_code);

// Attempts to reactivate Chrome for the specified |brand_code|. Returns false
// if reactivation fails, and places one of the REACTIVATE_ERROR_XXX values
// in |error_code| if |error_code| is non-null.
// |shell_mode| should be set to one of GCAPI_INVOKED_STANDARD_SHELL or
// GCAPI_INVOKED_UAC_ELEVATION depending on whether this method is invoked
// from an elevated or non-elevated process.
BOOL __stdcall ReactivateChrome(const wchar_t* brand_code,
                                int shell_mode,
                                DWORD* error_code);

// Returns true if a vendor may offer relaunch at this time. Returns false if
// the vendor may not offer relaunching at this time, and places one of the
// RELAUNCH_ERROR_XXX values in |error_code| if |error_code| is non-null. The
// installed brandcode must be in |partner_brandcode_list|. |shell_mode| should
// be set to one of GCAPI_INVOKED_STANDARD_SHELL or GCAPI_INVOKED_UAC_ELEVATION
// depending on whether this method is invoked from an elevated or non-elevated
// process.
BOOL __stdcall CanOfferRelaunch(const wchar_t** partner_brandcode_list,
                                int partner_brandcode_list_length,
                                int shell_mode,
                                DWORD* error_code);

// Returns true if a vendor may relaunch at this time (and stores that a
// relaunch was offered). Returns false if the vendor may not relaunch
// at this time, and places one of the RELAUNCH_ERROR_XXX values in |error_code|
// if |error_code| is non-null. As for |CanOfferRelaunch|, the installed
// brandcode must be in |partner_brandcode_list|. |shell_mode| should be set to
// one of GCAPI_INVOKED_STANDARD_SHELL or GCAPI_INVOKED_UAC_ELEVATION depending
// on whether this method is invoked from an elevated or non-elevated process.
// The |relaunch_brandcode| will be stored as the brandcode that was used for
// offering this relaunch.
BOOL __stdcall SetRelaunchOffered(const wchar_t** partner_brandcode_list,
                                  int partner_brandcode_list_length,
                                  const wchar_t* relaunch_brandcode,
                                  int shell_mode,
                                  DWORD* error_code);

// Function pointer type declarations to use with GetProcAddress.
typedef BOOL(__stdcall* GCCC_CompatibilityCheck)(BOOL, int, DWORD*);
typedef BOOL(__stdcall* GCCC_LaunchGC)();
typedef BOOL(__stdcall* GCCC_LaunchGoogleChromeInBackground)();
typedef BOOL(__stdcall* GCCC_LaunchGCWithDimensions)(int, int, int, int, bool);
typedef int(__stdcall* GCCC_GoogleChromeDaysSinceLastRun)();
typedef BOOL(__stdcall* GCCC_CanOfferReactivation)(const wchar_t*, int, DWORD*);
typedef BOOL(__stdcall* GCCC_ReactivateChrome)(const wchar_t*, int, DWORD*);
typedef BOOL(__stdcall* GCCC_CanOfferRelaunch)(const wchar_t**,
                                               int,
                                               int,
                                               DWORD*);
typedef BOOL(__stdcall* GCCC_SetRelaunchOffered)(const wchar_t**,
                                                 int,
                                                 const wchar_t*,
                                                 int,
                                                 DWORD*);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CHROME_INSTALLER_GCAPI_GCAPI_H_
