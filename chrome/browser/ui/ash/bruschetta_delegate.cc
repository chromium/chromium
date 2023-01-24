// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/bruschetta_delegate.h"

#include "chrome/browser/ui/views/bruschetta/bruschetta_installer_view.h"

void RunBruschettaInstaller(Profile* profile,
                            const guest_os::GuestId& guest_id) {
  BruschettaInstallerView::Show(profile, guest_id);
}
