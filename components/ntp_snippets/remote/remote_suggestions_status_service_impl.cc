// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_status_service_impl.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/ntp_snippets/content_suggestions_metrics.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

RemoteSuggestionsStatusServiceImpl::RemoteSuggestionsStatusServiceImpl(
    bool is_signed_in,
    PrefService* pref_service,
    const std::vector<std::string>& additional_toggle_prefs)
    : status_(RemoteSuggestionsStatus::EXPLICITLY_DISABLED),
      additional_toggle_prefs_(additional_toggle_prefs),
      is_signed_in_(is_signed_in),
      list_visible_during_session_(true),
      pref_service_(pref_service) {
  ntp_snippets::metrics::RecordRemoteSuggestionsProviderState(
      !IsExplicitlyDisabled());
}

RemoteSuggestionsStatusServiceImpl::~RemoteSuggestionsStatusServiceImpl() =
    default;

void RemoteSuggestionsStatusServiceImpl::Init(
    const StatusChangeCallback& callback) {
  DCHECK(status_change_callback_.is_null());

  status_change_callback_ = callback;

  list_visible_during_session_ =
      pref_service_->GetBoolean(feed::prefs::kArticlesListVisible);

  // Notify about the current state before registering the observer, to make
  // sure we don't get a double notification due to an undefined start state.
  RemoteSuggestionsStatus old_status = status_;
  status_ = GetStatusFromDeps();
  status_change_callback_.Run(old_status, status_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      feed::prefs::kEnableSnippets,
      base::BindRepeating(
          &RemoteSuggestionsStatusServiceImpl::OnSnippetsEnabledChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      feed::prefs::kArticlesListVisible,
      base::BindRepeating(
          &RemoteSuggestionsStatusServiceImpl::OnListVisibilityChanged,
          base::Unretained(this)));

  for (const std::string& additional_toggle_pref : additional_toggle_prefs_) {
    pref_change_registrar_.Add(
        additional_toggle_pref,
        base::BindRepeating(
            &RemoteSuggestionsStatusServiceImpl::OnSnippetsEnabledChanged,
            base::Unretained(this)));
  }
}

void RemoteSuggestionsStatusServiceImpl::OnSnippetsEnabledChanged() {
  OnStateChanged(GetStatusFromDeps());
}

void RemoteSuggestionsStatusServiceImpl::OnStateChanged(
    RemoteSuggestionsStatus new_status) {
  if (new_status == status_) {
    return;
  }

  status_change_callback_.Run(status_, new_status);
  status_ = new_status;
}

bool RemoteSuggestionsStatusServiceImpl::IsSignedIn() const {
  return is_signed_in_;
}

void RemoteSuggestionsStatusServiceImpl::OnSignInStateChanged(
    bool has_signed_in) {
  is_signed_in_ = has_signed_in;
  OnStateChanged(GetStatusFromDeps());
}

void RemoteSuggestionsStatusServiceImpl::OnListVisibilityChanged() {
  if (pref_service_->GetBoolean(feed::prefs::kArticlesListVisible)) {
    list_visible_during_session_ = true;
  }
  OnStateChanged(GetStatusFromDeps());
}

bool RemoteSuggestionsStatusServiceImpl::IsExplicitlyDisabled() const {
  if (!pref_service_->GetBoolean(feed::prefs::kEnableSnippets)) {
    DVLOG(1) << "[GetStatusFromDeps] Disabled via pref.";
    return true;
  }

  if (!list_visible_during_session_) {
    DVLOG(1) << "[GetStatusFromDeps] Disabled because articles list hidden.";
    return true;
  }

  // |additional_toggle_prefs_| will always be empty on Android.
  for (const std::string& additional_toggle_pref : additional_toggle_prefs_) {
    if (!pref_service_->GetBoolean(additional_toggle_pref)) {
      DVLOG(1) << "[GetStatusFromDeps] Disabled via additional pref";
      return true;
    }
  }

  return false;
}

RemoteSuggestionsStatus RemoteSuggestionsStatusServiceImpl::GetStatusFromDeps()
    const {
  if (IsExplicitlyDisabled()) {
    return RemoteSuggestionsStatus::EXPLICITLY_DISABLED;
  }

  DVLOG(1) << "[GetStatusFromDeps] Enabled, signed "
           << (IsSignedIn() ? "in" : "out");
  return IsSignedIn() ? RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN
                      : RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT;
}

}  // namespace ntp_snippets
