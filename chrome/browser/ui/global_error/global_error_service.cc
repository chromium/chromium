// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error_service.h"

#include <stddef.h>

#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"

GlobalErrorService::GlobalErrorService() = default;

GlobalErrorService::~GlobalErrorService() = default;

void GlobalErrorService::AddObserver(GlobalErrorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GlobalErrorService::RemoveObserver(GlobalErrorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void GlobalErrorService::AddGlobalError(std::unique_ptr<GlobalError> error) {
  DCHECK(error);
  GlobalError* error_ptr = error.get();
  owned_errors_[error_ptr] = std::move(error);
  AddUnownedGlobalError(error_ptr);
}

void GlobalErrorService::AddUnownedGlobalError(GlobalError* error) {
  DCHECK(error);
  all_errors_.push_back(error);
  NotifyErrorsChanged();
}

std::unique_ptr<GlobalError> GlobalErrorService::RemoveGlobalError(
    GlobalError* error_ptr) {
  std::unique_ptr<GlobalError> ptr = std::move(owned_errors_[error_ptr]);
  owned_errors_.erase(error_ptr);
  RemoveUnownedGlobalError(error_ptr);
  return ptr;
}

void GlobalErrorService::RemoveUnownedGlobalError(GlobalError* error) {
  DCHECK(owned_errors_.find(error) == owned_errors_.end());
  all_errors_.erase(base::ranges::find(all_errors_, error));
  GlobalErrorBubbleViewBase* bubble = error->GetBubbleView();
  if (bubble)
    bubble->CloseBubbleView();
  NotifyErrorsChanged();
}

GlobalError* GlobalErrorService::GetGlobalErrorByMenuItemCommandID(
    int command_id) const {
  for (GlobalError* error : all_errors_) {
    if (error->HasMenuItem() && command_id == error->MenuItemCommandID())
      return error;
  }

  return nullptr;
}

GlobalError*
GlobalErrorService::GetHighestSeverityGlobalErrorWithAppMenuItem() const {
  GlobalError::Severity highest_severity = GlobalError::SEVERITY_LOW;
  GlobalError* highest_severity_error = nullptr;

  for (GlobalError* error : all_errors_) {
    if (error->HasMenuItem()) {
      if (!highest_severity_error || error->GetSeverity() > highest_severity) {
        highest_severity = error->GetSeverity();
        highest_severity_error = error;
      }
    }
  }

  return highest_severity_error;
}

GlobalError* GlobalErrorService::GetFirstGlobalErrorWithBubbleView() const {
  for (GlobalError* error : all_errors_) {
    if (error->HasBubbleView() && !error->HasShownBubbleView())
      return error;
  }
  return nullptr;
}

void GlobalErrorService::NotifyErrorsChanged() {
  for (auto& observer : observer_list_)
    observer.OnGlobalErrorsChanged();
}
