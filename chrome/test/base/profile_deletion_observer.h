// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PROFILE_DELETION_OBSERVER_H_
#define CHROME_TEST_BASE_PROFILE_DELETION_OBSERVER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"

class ProfileDeletionObserver : public ProfileAttributesStorage::Observer {
 public:
  ProfileDeletionObserver();
  ~ProfileDeletionObserver() override;

  // Synchronously waits until a single Profile is removed from
  // ProfileAttributesStorage, marking the end of a
  // ScheduleProfileForDeletion() operation.
  void Wait();

  // ProfileAttributesStorage::Observer:
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;

 private:
  base::RunLoop run_loop_;
};

#endif  // CHROME_TEST_BASE_PROFILE_DELETION_OBSERVER_H_
