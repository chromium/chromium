// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_FAKE_PROFILE_MANAGER_H_
#define CHROME_TEST_BASE_FAKE_PROFILE_MANAGER_H_

#include <memory>

#include "chrome/browser/profiles/profile_manager.h"

class Profile;

namespace base {
class FilePath;
}

// Fake implementation of ProfileManager that creates a `TestingProfile` instead
// of a regular `Profile`.
//
// This class is useful for testing code that creates new profiles, when
// `TestingProfile` cannot be created directly in the test and injected into
// ProfileManager via `ProfileManager::RegisterTestingProfile()`.
//
// To override the `ProfileManager` instance in tests, call
//   `TestingBrowserProcess::GetGlobal()->SetProfileManager(
//                   std::make_unique<FakeProfileManager>(user_data_dir));`
class FakeProfileManager : public ProfileManagerWithoutInit {
 public:
  explicit FakeProfileManager(const base::FilePath& user_data_dir);
  ~FakeProfileManager() override;

  // Allows subclasses to modify the TestingProfile creation. Used for both
  // synchronous and asynchronous profile creation. By default returns a new
  // `TestingProfile` initialized with `path` and `delegate`.
  virtual std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Delegate* delegate,
      Profile::CreateMode create_mode);

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) final;
  std::unique_ptr<Profile> CreateProfileAsyncHelper(
      const base::FilePath& path) final;
};

#endif  // CHROME_TEST_BASE_FAKE_PROFILE_MANAGER_H_
