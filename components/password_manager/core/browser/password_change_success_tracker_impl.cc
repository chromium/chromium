// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"

#include "base/notreached.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace password_manager {

PasswordChangeSuccessTrackerImpl::PasswordChangeSuccessTrackerImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  // Check whether the saved entries belong to an old version. If so,
  // remove all old flows.
  if (pref_service->GetInteger(prefs::kPasswordChangeSuccessTrackerVersion) <
      kTrackerVersion) {
    pref_service->SetInteger(prefs::kPasswordChangeSuccessTrackerVersion,
                             kTrackerVersion);
    pref_service->ClearPref(prefs::kPasswordChangeSuccessTrackerFlows);
  }
}

PasswordChangeSuccessTrackerImpl::~PasswordChangeSuccessTrackerImpl() = default;

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowStarted(
    const GURL& url,
    const std::string& username,
    StartEvent event_type,
    EntryPoint entry_point) {
  // TODO(crbug.com/1281844): Implement metrics recoding.
  NOTIMPLEMENTED();
}

void PasswordChangeSuccessTrackerImpl::OnManualChangePasswordFlowStarted(
    const GURL& url,
    const std::string& username,
    EntryPoint entry_point) {
  // TODO(crbug.com/1281844): Implement metrics recoding.
  NOTIMPLEMENTED();
}

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowModified(
    const GURL& url,
    StartEvent new_event_type) {
  // TODO(crbug.com/1281844): Implement metrics recoding.
  NOTIMPLEMENTED();
}

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowModified(
    const GURL& url,
    const std::string& username,
    StartEvent new_event_type) {
  // TODO(crbug.com/1281844): Implement metrics recoding.
  NOTIMPLEMENTED();
}

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowCompleted(
    const GURL& url,
    const std::string& username,
    EndEvent event_type) {
  // TODO(crbug.com/1281844): Implement metrics recoding.
  NOTIMPLEMENTED();
}

}  // namespace password_manager
