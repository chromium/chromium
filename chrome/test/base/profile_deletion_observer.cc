// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/profile_deletion_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"

ProfileDeletionObserver::ProfileDeletionObserver() {
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .AddObserver(this);
}

ProfileDeletionObserver::~ProfileDeletionObserver() {
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .RemoveObserver(this);
}

void ProfileDeletionObserver::Wait() {
  run_loop_.Run();
}

// ProfileAttributesStorage::Observer:
void ProfileDeletionObserver::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  run_loop_.Quit();
}
