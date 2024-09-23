// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/fake_profile_manager.h"

#include "base/files/file_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"

FakeProfileManager::FakeProfileManager(const base::FilePath& user_data_dir)
    : ProfileManagerWithoutInit(user_data_dir) {}

FakeProfileManager::~FakeProfileManager() = default;

std::unique_ptr<TestingProfile> FakeProfileManager::BuildTestingProfile(
    const base::FilePath& path,
    Delegate* delegate,
    Profile::CreateMode create_mode) {
  return std::make_unique<TestingProfile>(path, delegate, create_mode);
}

std::unique_ptr<Profile> FakeProfileManager::CreateProfileHelper(
    const base::FilePath& path) {
  if (!base::PathExists(path) && !base::CreateDirectory(path))
    return nullptr;
  return BuildTestingProfile(path, this, Profile::CreateMode::kSynchronous);
}

std::unique_ptr<Profile> FakeProfileManager::CreateProfileAsyncHelper(
    const base::FilePath& path) {
  // SingleThreadTaskRunner::GetCurrentDefault() is TestingProfile's "async"
  // IOTaskRunner (ref. TestingProfile::GetIOTaskRunner()).
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::CreateDirectory), path));

  return BuildTestingProfile(path, this, Profile::CreateMode::kAsynchronous);
}
