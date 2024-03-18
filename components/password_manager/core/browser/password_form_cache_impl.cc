// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_cache_impl.h"

#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"

namespace password_manager {

PasswordFormCacheImpl::PasswordFormCacheImpl() = default;

PasswordFormCacheImpl::~PasswordFormCacheImpl() = default;

bool PasswordFormCacheImpl::HasPasswordForm(
    PasswordManagerDriver* driver,
    autofill::FormRendererId form_id) const {
  return GetMatchedManager(driver, form_id) != nullptr;
}

bool PasswordFormCacheImpl::HasPasswordForm(
    PasswordManagerDriver* driver,
    autofill::FieldRendererId field_id) const {
  return GetMatchedManager(driver, field_id) != nullptr;
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
  form_managers_.emplace_back(std::move(manager));
}

void PasswordFormCacheImpl::ResetSubmittedManager() {
  auto submitted_manager =
      base::ranges::find_if(form_managers_, &PasswordFormManager::is_submitted);
  if (submitted_manager != form_managers_.end()) {
    form_managers_.erase(submitted_manager);
  }
}

PasswordFormManager* PasswordFormCacheImpl::GetSubmittedManager() {
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

}  // namespace password_manager
