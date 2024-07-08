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

  // Verify that there are no pref observers when we shut down.
  for (const auto& observer_list : pref_observers_) {
    if (observer_list.second.begin() != observer_list.second.end()) {
      // Generally, there should not be any subscribers left when the profile
      // is destroyed because a) those may indicate that the subscriber class
      // maintains an active pointer to the profile that might be used for
      // accessing a destroyed profile and b) those subscribers will try to
      // unsubscribe from a PrefService that has been destroyed with the
      // profile.
      // There is one exception that is safe: Static objects that are leaked
      // on process termination, if these objects just subscribe to preferences
      // and never access the profile after destruction. As these objects are
      // leaked on termination, it is guaranteed that they don't attempt to
      // unsubscribe.
      const auto& pref_name = observer_list.first;
      std::string message = base::StrCat(
          {"Pref observer for ", pref_name, " found at shutdown."});
      LOG(WARNING) << message;
      DEBUG_ALIAS_FOR_CSTR(aliased_message, message.c_str(), 128);

      // TODO(crbug.com/942491, 946668, 945772) The following code collects
      // stacktraces that show how the profile is destroyed that owns
      // preferences which are known to have subscriptions outliving the
      // profile.
      if (
          // For DbusAppmenu, crbug.com/946668
          pref_name == "bookmark_bar.show_on_all_tabs" ||
          // For BrowserWindowPropertyManager, crbug.com/942491
          pref_name == "profile.icon_version") {
        base::debug::DumpWithoutCrashing();
      }
    }
  }

  // Same for initialization observers.
  if (!init_observers_.empty())
    LOG(WARNING) << "Init observer found at shutdown.";

  pref_observers_.clear();
  init_observers_.clear();
}

void PrefNotifierImpl::AddPrefObserver(std::string_view path,
                                       PrefObserver* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Add the pref observer. ObserverList hits a DCHECK if it already is
  // in the list.
  pref_observers_[std::string(path)].AddObserver(obs);
}

void PrefNotifierImpl::RemovePrefObserver(std::string_view path,
                                          PrefObserver* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    return;
  }

  PrefObserverList& observer_list = observer_iterator->second;
  observer_list.RemoveObserver(obs);
}

void PrefNotifierImpl::AddPrefObserverAllPrefs(PrefObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  all_prefs_pref_observers_.AddObserver(observer);
}

void PrefNotifierImpl::RemovePrefObserverAllPrefs(PrefObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  all_prefs_pref_observers_.RemoveObserver(observer);
}

void PrefNotifierImpl::AddInitObserver(base::OnceCallback<void(bool)> obs) {
  init_observers_.push_back(std::move(obs));
}

void PrefNotifierImpl::OnPreferenceChanged(std::string_view path) {
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

  // Only send notifications for registered preferences.
  if (!pref_service_->FindPreference(path))
    return;

  // Fire observers for any preference change.
  for (PrefObserver& observer : all_prefs_pref_observers_) {
    observer.OnPreferenceChanged(pref_service_, path);
  }

  auto observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end())
    return;

  for (PrefObserver& observer : observer_iterator->second) {
    observer.OnPreferenceChanged(pref_service_, path);
  }
}

void PrefNotifierImpl::SetPrefService(PrefService* pref_service) {
  DCHECK(pref_service_ == nullptr);
  pref_service_ = pref_service;
}
