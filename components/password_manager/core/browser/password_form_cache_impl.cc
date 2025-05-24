// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_cache_impl.h"

#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"

namespace password_manager {

PasswordFormCacheImpl::PasswordFormCacheImpl() = default;

PasswordFormCacheImpl::~PasswordFormCacheImpl() = default;

const PasswordForm* PasswordFormCacheImpl::GetPasswordForm(
    PasswordManagerDriver* driver,
    autofill::FormRendererId form_id) const {
  const PasswordFormManager* form_manager = GetMatchedManager(driver, form_id);
  return form_manager ? form_manager->GetParsedObservedForm() : nullptr;
}

const PasswordForm* PasswordFormCacheImpl::GetPasswordForm(
    PasswordManagerDriver* driver,
    autofill::FieldRendererId field_id) const {
  const PasswordFormManager* form_manager = GetMatchedManager(driver, field_id);
  return form_manager ? form_manager->GetParsedObservedForm() : nullptr;
}

PasswordFormManager* PasswordFormCacheImpl::GetMatchedManager(
    PasswordManagerDriver* driver,
    autofill::FormRendererId form_id) const {
  for (auto& form_manager : form_managers_) {
    if (form_manager->DoesManage(form_id, driver)) {
      return form_manager.get();
    }
  }
  return nullptr;
}

PasswordFormManager* PasswordFormCacheImpl::GetMatchedManager(
    PasswordManagerDriver* driver,
    autofill::FieldRendererId field_id) const {
  for (auto& form_manager : form_managers_) {
    if (form_manager->DoesManage(field_id, driver)) {
      return form_manager.get();
    }
  }
  return nullptr;
}

void PasswordFormCacheImpl::AddFormManager(
    std::unique_ptr<PasswordFormManager> manager) {
  if (form_manager_observer_) {
    manager->SetObserver(form_manager_observer_);
  }
  form_managers_.emplace_back(std::move(manager));
}

void PasswordFormCacheImpl::ResetSubmittedManager() {
  auto submitted_manager =
      std::ranges::find_if(form_managers_, &PasswordFormManager::is_submitted);
  if (submitted_manager != form_managers_.end()) {
    form_managers_.erase(submitted_manager);
  }
}

PasswordFormManager* PasswordFormCacheImpl::GetSubmittedManager() const {
  for (const std::unique_ptr<PasswordFormManager>& manager : form_managers_) {
    if (manager->is_submitted()) {
      return manager.get();
    }
  }
  return nullptr;
}

std::unique_ptr<PasswordFormManager>
PasswordFormCacheImpl::MoveOwnedSubmittedManager() {
  for (auto iter = form_managers_.begin(); iter != form_managers_.end();
       ++iter) {
    if ((*iter)->is_submitted()) {
      std::unique_ptr<PasswordFormManager> submitted_manager = std::move(*iter);
      form_managers_.erase(iter);

      // After PasswordFormManager is removed from cache it's impossible to
      // reset observation. Thus, it's safer to stop observing immediately.
      submitted_manager->ResetObserver();
      return submitted_manager;
    }
  }
  return nullptr;
}

void PasswordFormCacheImpl::Clear() {
  form_managers_.clear();
}

bool PasswordFormCacheImpl::IsEmpty() const {
  return form_managers_.empty();
}

base::span<const std::unique_ptr<PasswordFormManager>>
PasswordFormCacheImpl::GetFormManagers() const {
  return base::span(form_managers_);
}

void PasswordFormCacheImpl::SetObserver(
    base::WeakPtr<PasswordFormManagerObserver> observer) {
  form_manager_observer_ = observer;
}

void PasswordFormCacheImpl::ResetObserver() {
  form_manager_observer_.reset();
  for (const std::unique_ptr<PasswordFormManager>& manager : form_managers_) {
    manager->ResetObserver();
  }
}

}  // namespace password_manager
