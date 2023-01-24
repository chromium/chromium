// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BRUSCHETTA_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_BRUSCHETTA_DELEGATE_H_

#include "chrome/browser/ash/guest_os/guest_id.h"

class Profile;

void RunBruschettaInstaller(Profile* profile,
                            const guest_os::GuestId& guest_id);

#endif  // CHROME_BROWSER_UI_ASH_BRUSCHETTA_DELEGATE_H_
