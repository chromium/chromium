// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_UTIL_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_UTIL_H_

namespace google_update {

// Tell Google Update that an uninstall has taken place.  This gives it a chance
// to uninstall itself straight away if no more products are installed on the
// system rather than waiting for the next time the scheduled task runs.
// Returns false if Google Update could not be executed, or times out.
bool UninstallGoogleUpdate(bool system_install);

// Run setup.exe to attempt to reenable updates for for Chrome while elevating
// if needed. Setup.exe will call into
// GoogleUpdateSettings::ReenableAutoupdatesForApp() to do the work.
void ElevateIfNeededToReenableUpdates();

}  // namespace google_update

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_UTIL_H_
