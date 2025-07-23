// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_NOTIFIER_IMPL_H_
#define COMPONENTS_PREFS_PREF_NOTIFIER_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/prefs/pref_notifier.h"
#include "components/prefs/prefs_export.h"
#include "components/prefs/transparent_unordered_string_map.h"

class PrefService;
namespace base {
class CallbackListSubscription;
}

// The PrefNotifier implementation used by the PrefService.
class COMPONENTS_PREFS_EXPORT PrefNotifierImpl : public PrefNotifier {
 public:
  using PrefChangedCallback =
      base::RepeatingCallback<void(PrefService*, std::string_view)>;

  PrefNotifierImpl();
  explicit PrefNotifierImpl(PrefService* pref_service);

  PrefNotifierImpl(const PrefNotifierImpl&) = delete;
  PrefNotifierImpl& operator=(const PrefNotifierImpl&) = delete;

  ~PrefNotifierImpl() override;

  // Registers the callback to be invoked if the pref at the given path
  // changes. The callback is automatically unregistered if the returned
  // CallbackListSubscription is destroyed.
  base::CallbackListSubscription AddPrefChangedCallback(
      std::string_view path,
      PrefChangedCallback callback);

  // These callbacks are called for any pref changes.
  //
  // AVOID ADDING THESE. See the long comment in the identically-named
  // functions on PrefService for background.
  base::CallbackListSubscription AddAllPrefsChangedCallback(
      PrefChangedCallback callback);

  // We run the callback once, when initialization completes. The bool
  // parameter will be set to true for successful initialization,
  // false for unsuccessful.
  void AddInitObserver(base::OnceCallback<void(bool)> observer);

  void SetPrefService(PrefService* pref_service);

  // PrefNotifier overrides.
  void OnPreferenceChanged(std::string_view pref_name) override;

 protected:
  // PrefNotifier overrides.
  void OnInitializationCompleted(bool succeeded) override;

 private:
  // A map from pref names to the list of registered callbacks. Callbacks get
  // fired in the order they are added.
  using PrefChangedCallbackList =
      base::RepeatingCallbackList<void(PrefService*, std::string_view)>;
  using PrefChangedCallbackMap =
      TransparentUnorderedStringMap<PrefChangedCallbackList>;
  using PrefInitObserverList = std::list<base::OnceCallback<void(bool)>>;

  // For the given pref_name, notify any callbacks of the pref. Virtual so it
  // can be mocked for unit testing.
  virtual void NotifyCallbacks(std::string_view path);

  // Invoked when callbacks are removed for `path`.
  void OnCallbacksRemoved(const std::string& path);

  // Weak reference; the notifier is owned by the PrefService.
  raw_ptr<PrefService> pref_service_;

  PrefChangedCallbackMap pref_changed_callbacks_;
  PrefInitObserverList init_observers_;

  // Observers for changes to any preference.
  PrefChangedCallbackList all_prefs_changed_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // COMPONENTS_PREFS_PREF_NOTIFIER_IMPL_H_
