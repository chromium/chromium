// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/password_change/empty_website_login_manager_impl.h"

#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace autofill_assistant {

EmptyWebsiteLoginManagerImpl::EmptyWebsiteLoginManagerImpl() = default;
EmptyWebsiteLoginManagerImpl::~EmptyWebsiteLoginManagerImpl() = default;

void EmptyWebsiteLoginManagerImpl::GetLoginsForUrl(
    const GURL& url,
    base::OnceCallback<void(std::vector<Login>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(std::vector<Login>());
}

void EmptyWebsiteLoginManagerImpl::GetPasswordForLogin(
    const Login& login,
    base::OnceCallback<void(bool, std::string)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(false, std::string());
}

void EmptyWebsiteLoginManagerImpl::DeletePasswordForLogin(
    const Login& login,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(false);
}

void EmptyWebsiteLoginManagerImpl::GetGetLastTimePasswordUsed(
    const Login& login,
    base::OnceCallback<void(absl::optional<base::Time>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(absl::nullopt);
}

void EmptyWebsiteLoginManagerImpl::EditPasswordForLogin(
    const Login& login,
    const std::string& new_password,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(false);
}

absl::optional<std::string> EmptyWebsiteLoginManagerImpl::GeneratePassword(
    content::RenderFrameHost* rfh,
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature,
    uint64_t max_length) {
  return absl::nullopt;
}

const std::string& EmptyWebsiteLoginManagerImpl::GetGeneratedPassword() {
  return generated_password_;
}

void EmptyWebsiteLoginManagerImpl::PresaveGeneratedPassword(
    const Login& login,
    const std::string& password,
    const autofill::FormData& form_data,
    base::OnceCallback<void()> callback) {
  std::move(callback).Run();
}

bool EmptyWebsiteLoginManagerImpl::ReadyToSaveGeneratedPassword() {
  return false;
}

void EmptyWebsiteLoginManagerImpl::SaveGeneratedPassword() {}

void EmptyWebsiteLoginManagerImpl::ResetPendingCredentials() {}

bool EmptyWebsiteLoginManagerImpl::ReadyToSaveSubmittedPassword() {
  return false;
}

bool EmptyWebsiteLoginManagerImpl::SubmittedPasswordIsSame() {
  return false;
}

void EmptyWebsiteLoginManagerImpl::CheckWhetherSubmittedCredentialIsLeaked(
    SavePasswordLeakDetectionDelegate::Callback callback,
    base::TimeDelta timeout) {
  std::move(callback).Run(
      LeakDetectionStatus(LeakDetectionStatusCode::DISABLED), false);
}

bool EmptyWebsiteLoginManagerImpl::SaveSubmittedPassword() {
  return false;
}

}  // namespace autofill_assistant
