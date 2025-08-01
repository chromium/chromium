// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_notifier_impl.h"

#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_service.h"

PrefNotifierImpl::PrefNotifierImpl() : pref_service_(nullptr) {}

PrefNotifierImpl::PrefNotifierImpl(PrefService* service)
    : pref_service_(service) {
}

PrefNotifierImpl::~PrefNotifierImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Verify that there are no initialization observers.
  if (!init_observers_.empty())
    LOG(WARNING) << "Init observer found at shutdown.";

  init_observers_.clear();
}

void PrefNotifierImpl::AddPrefObserver(std::string_view path,
                                       PrefObserver* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);

  // Add the pref observer. ObserverList hits a DCHECK if it already is
  // in the list.
  pref_observers_[std::string(path)].AddObserver(obs);
}

void PrefNotifierImpl::RemovePrefObserver(std::string_view path,
                                          PrefObserver* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);

  auto iterator = pref_observers_.find(path);
  if (iterator == pref_observers_.end()) {
    return;
  }

  iterator->second.RemoveObserver(obs);
}

void PrefNotifierImpl::AddPrefObserverAllPrefs(PrefObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);

  all_prefs_pref_observers_.AddObserver(observer);
}

void PrefNotifierImpl::RemovePrefObserverAllPrefs(PrefObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);

  all_prefs_pref_observers_.RemoveObserver(observer);
}

void PrefNotifierImpl::AddInitObserver(base::OnceCallback<void(bool)> obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_observers_.push_back(std::move(obs));
}

void PrefNotifierImpl::OnPreferenceChanged(std::string_view path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);

  FireObservers(path);
}

void PrefNotifierImpl::OnInitializationCompleted(bool succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We must move init_observers_ to a local variable before we run
  // observers, or we can end up in this method re-entrantly before
  // clearing the observers list.
  PrefInitObserverList observers;
  std::swap(observers, init_observers_);

  for (auto& observer : observers)
    std::move(observer).Run(succeeded);
}

void PrefNotifierImpl::FireObservers(std::string_view path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);

  // Only send notifications for registered preferences.
  if (!pref_service_->FindPreference(path))
    return;

  // Fire observers for any preference change.
  for (PrefObserver& pref_observer : all_prefs_pref_observers_) {
    pref_observer.OnPreferenceChanged(pref_service_, path);
  }

  auto iterator = pref_observers_.find(path);
  if (iterator != pref_observers_.end()) {
    for (PrefObserver& observer : iterator->second) {
      observer.OnPreferenceChanged(pref_service_, path);
    }
  }
}

void PrefNotifierImpl::SetPrefService(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_ == nullptr);
  pref_service_ = pref_service;
}

void PrefNotifierImpl::OnServiceDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);

  for (PrefObserver& pref_observer : all_prefs_pref_observers_) {
    pref_observer.OnServiceDestroyed(pref_service_);
  }
  DCHECK(all_prefs_pref_observers_.empty());

  for (auto& [_, observer_list] : pref_observers_) {
    for (PrefObserver& pref_observer : observer_list) {
      pref_observer.OnServiceDestroyed(pref_service_);
    }
    DCHECK(observer_list.empty());
  }

  pref_observers_.clear();
  pref_service_ = nullptr;
}
