// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_notifier_impl.h"

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
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
  for (const auto& observer_list : pref_changed_callbacks_) {
    if (!observer_list.second.empty()) {
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

  pref_changed_callbacks_.clear();
  init_observers_.clear();
}

base::CallbackListSubscription PrefNotifierImpl::AddPrefChangedCallback(
    std::string_view path,
    PrefChangedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iterator = pref_changed_callbacks_.find(path);
  if (iterator == pref_changed_callbacks_.end()) {
    bool inserted = false;
    std::tie(iterator, inserted) = pref_changed_callbacks_.emplace(
        std::piecewise_construct, std::forward_as_tuple(path),
        std::forward_as_tuple());
    DCHECK(inserted);

    // Set a removal callback in order to remove the mapping when
    // the last registered observer for `path` is removed. This
    // avoid unbounded growth of the map (it will be limited to
    // the maximum size of different preferences observer at the
    // same time).
    iterator->second.set_removal_callback(
        base::BindRepeating(&PrefNotifierImpl::OnCallbacksRemoved,
                            base::Unretained(this), std::string(path)));
  }

  DCHECK(iterator != pref_changed_callbacks_.end());
  return iterator->second.Add(std::move(callback));
}

base::CallbackListSubscription PrefNotifierImpl::AddAllPrefsChangedCallback(
    PrefChangedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return all_prefs_changed_callbacks_.Add(std::move(callback));
}

void PrefNotifierImpl::AddInitObserver(base::OnceCallback<void(bool)> obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_observers_.push_back(std::move(obs));
}

void PrefNotifierImpl::OnPreferenceChanged(std::string_view path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyCallbacks(path);
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

void PrefNotifierImpl::NotifyCallbacks(std::string_view path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only send notifications for registered preferences.
  if (!pref_service_->FindPreference(path))
    return;

  // Fire observers for any preference change.
  all_prefs_changed_callbacks_.Notify(pref_service_, path);

  auto iterator = pref_changed_callbacks_.find(path);
  if (iterator != pref_changed_callbacks_.end()) {
    iterator->second.Notify(pref_service_, path);
  }
}

void PrefNotifierImpl::SetPrefService(PrefService* pref_service) {
  DCHECK(pref_service_ == nullptr);
  pref_service_ = pref_service;
}

void PrefNotifierImpl::OnCallbacksRemoved(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iterator = pref_changed_callbacks_.find(path);
  DCHECK(iterator != pref_changed_callbacks_.end());
  if (iterator->second.empty()) {
    pref_changed_callbacks_.erase(iterator);
  }
}
