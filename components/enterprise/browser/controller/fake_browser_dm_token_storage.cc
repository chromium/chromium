// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/enterprise/browser/enterprise_switches.h"

namespace policy {

FakeBrowserDMTokenStorage::FakeBrowserDMTokenStorage() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableChromeBrowserCloudManagement);
  BrowserDMTokenStorage::SetForTesting(this);
  delegate_ = std::make_unique<MockDelegate>();
}

FakeBrowserDMTokenStorage::FakeBrowserDMTokenStorage(
    const std::string& client_id,
    const std::string& enrollment_token,
    const std::string& dm_token,
    bool enrollment_error_option) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableChromeBrowserCloudManagement);
  BrowserDMTokenStorage::SetForTesting(this);
  delegate_ = std::make_unique<MockDelegate>(client_id, enrollment_token,
                                             dm_token, enrollment_error_option);
}

FakeBrowserDMTokenStorage::~FakeBrowserDMTokenStorage() {
  BrowserDMTokenStorage::SetForTesting(nullptr);
}

void FakeBrowserDMTokenStorage::SetClientId(const std::string& client_id) {
  static_cast<MockDelegate*>(delegate_.get())->SetClientId(client_id);
}

void FakeBrowserDMTokenStorage::SetEnrollmentToken(
    const std::string& enrollment_token) {
  static_cast<MockDelegate*>(delegate_.get())
      ->SetEnrollmentToken(enrollment_token);
}

void FakeBrowserDMTokenStorage::SetDMToken(const std::string& dm_token) {
  static_cast<MockDelegate*>(delegate_.get())->SetDMToken(dm_token);
}

void FakeBrowserDMTokenStorage::SetEnrollmentErrorOption(bool option) {
  static_cast<MockDelegate*>(delegate_.get())->SetEnrollmentErrorOption(option);
}

void FakeBrowserDMTokenStorage::EnableStorage(bool storage_enabled) {
  static_cast<MockDelegate*>(delegate_.get())->EnableStorage(storage_enabled);
}

FakeBrowserDMTokenStorage::MockDelegate::MockDelegate() = default;

FakeBrowserDMTokenStorage::MockDelegate::MockDelegate(
    const std::string& client_id,
    const std::string& enrollment_token,
    const std::string& dm_token,
    bool enrollment_error_option)
    : client_id_(client_id),
      enrollment_token_(enrollment_token),
      dm_token_(dm_token),
      enrollment_error_option_(enrollment_error_option) {}

FakeBrowserDMTokenStorage::MockDelegate::~MockDelegate() = default;

void FakeBrowserDMTokenStorage::MockDelegate::SetClientId(
    const std::string& client_id) {
  client_id_ = client_id;
}

void FakeBrowserDMTokenStorage::MockDelegate::SetEnrollmentToken(
    const std::string& enrollment_token) {
  enrollment_token_ = enrollment_token;
}

void FakeBrowserDMTokenStorage::MockDelegate::SetDMToken(
    const std::string& dm_token) {
  dm_token_ = dm_token;
}

void FakeBrowserDMTokenStorage::MockDelegate::SetEnrollmentErrorOption(
    bool option) {
  enrollment_error_option_ = option;
}

void FakeBrowserDMTokenStorage::MockDelegate::EnableStorage(
    bool storage_enabled) {
  storage_enabled_ = storage_enabled;
}

std::string FakeBrowserDMTokenStorage::MockDelegate::InitClientId() {
  return client_id_;
}

std::string FakeBrowserDMTokenStorage::MockDelegate::InitEnrollmentToken() {
  return enrollment_token_;
}

std::string FakeBrowserDMTokenStorage::MockDelegate::InitDMToken() {
  return dm_token_;
}

bool FakeBrowserDMTokenStorage::MockDelegate::InitEnrollmentErrorOption() {
  return enrollment_error_option_;
}

bool FakeBrowserDMTokenStorage::MockDelegate::CanInitEnrollmentToken() const {
  return true;
}

BrowserDMTokenStorage::StoreTask
FakeBrowserDMTokenStorage::MockDelegate::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  return base::BindOnce([](bool enabled) -> bool { return enabled; },
                        storage_enabled_);
}

BrowserDMTokenStorage::StoreTask
FakeBrowserDMTokenStorage::MockDelegate::DeleteDMTokenTask(
    const std::string& client_id) {
  return base::BindOnce([](bool enabled) -> bool { return enabled; },
                        storage_enabled_);
}

scoped_refptr<base::TaskRunner>
FakeBrowserDMTokenStorage::MockDelegate::SaveDMTokenTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace policy
