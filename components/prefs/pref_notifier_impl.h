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

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/prefs/pref_notifier.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/prefs_export.h"
#include "components/prefs/transparent_unordered_string_map.h"

class PrefService;

// The PrefNotifier implementation used by the PrefService.
class COMPONENTS_PREFS_EXPORT PrefNotifierImpl : public PrefNotifier {
 public:
  PrefNotifierImpl();
  explicit PrefNotifierImpl(PrefService* pref_service);

  PrefNotifierImpl(const PrefNotifierImpl&) = delete;
  PrefNotifierImpl& operator=(const PrefNotifierImpl&) = delete;

  ~PrefNotifierImpl() override;

  // If the pref at the given path changes, we call the observer's
  // OnPreferenceChanged method.
  void AddPrefObserver(std::string_view path, PrefObserver* observer);
  void RemovePrefObserver(std::string_view path, PrefObserver* observer);

  // These observers are called for any pref changes.
  //
  // AVOID ADDING THESE. See the long comment in the identically-named
  // functions on PrefService for background.
  void AddPrefObserverAllPrefs(PrefObserver* observer);
  void RemovePrefObserverAllPrefs(PrefObserver* observer);

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

  // A map from pref names to a list of observers. Observers get fired in the
  // order they are added. These should only be accessed externally for unit
  // testing.
  using PrefObserverList = base::ObserverList<PrefObserver>::Unchecked;
  using PrefObserverMap = TransparentUnorderedStringMap<PrefObserverList>;
  using PrefInitObserverList = std::list<base::OnceCallback<void(bool)>>;

  const PrefObserverMap* pref_observers() const { return &pref_observers_; }

 private:
  // For the given pref_name, fire any observer of the pref. Virtual so it can
  // be mocked for unit testing.
  virtual void FireObservers(std::string_view path);

  // Weak reference; the notifier is owned by the PrefService.
  raw_ptr<PrefService> pref_service_;

  PrefObserverMap pref_observers_;
  PrefInitObserverList init_observers_;

  // Observers for changes to any preference.
  PrefObserverList all_prefs_pref_observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // COMPONENTS_PREFS_PREF_NOTIFIER_IMPL_H_
