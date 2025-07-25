// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_CHANGE_REGISTRAR_H_
#define COMPONENTS_PREFS_PREF_CHANGE_REGISTRAR_H_

#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/prefs_export.h"

class PrefService;

// Automatically manages the registration of one or more pref change observers
// with a PrefStore. When the Registrar is destroyed, all registered observers
// are automatically unregistered with the PrefStore.
class COMPONENTS_PREFS_EXPORT PrefChangeRegistrar final : public PrefObserver {
 public:
  // You can register this type of callback if you need to know the
  // path of the preference that is changing.
  using NamedChangeCallback = base::RepeatingCallback<void(const std::string&)>;

  PrefChangeRegistrar();

  PrefChangeRegistrar(const PrefChangeRegistrar&) = delete;
  PrefChangeRegistrar& operator=(const PrefChangeRegistrar&) = delete;

  ~PrefChangeRegistrar();

  // Must be called before adding or removing observers. Can be called more
  // than once as long as the value of |service| doesn't change.
  void Init(PrefService* service);

  // Removes all observers and clears the reference to `PrefService`.
  // `Init` must be called before adding or removing any observers.
  void Reset();

  // Adds a pref observer for the specified pref |path| and |obs| observer
  // object. All registered observers will be automatically unregistered
  // when the registrar's destructor is called.
  //
  // The second version binds a callback that will receive the path of
  // the preference that is changing as its parameter.
  //
  // Only one observer may be registered per path.
  void Add(std::string_view path, const base::RepeatingClosure& obs);
  void Add(std::string_view path, const NamedChangeCallback& obs);

  // Removes the pref observer registered for |path|.
  void Remove(std::string_view path);

  // Removes all observers that have been previously added with a call to Add.
  void RemoveAll();

  // Returns true if no pref observers are registered.
  bool IsEmpty() const;

  // Check whether |pref| is in the set of preferences being observed.
  bool IsObserved(std::string_view pref);

  // Return the PrefService for this registrar.
  PrefService* prefs();
  const PrefService* prefs() const;

 private:
  // PrefObserver:
  void OnPreferenceChanged(PrefService* service,
                           std::string_view pref_name) override;

  static void InvokeUnnamedCallback(const base::RepeatingClosure& callback,
                                    const std::string& pref_name);

  using ObserverMap = std::map<std::string, NamedChangeCallback, std::less<>>;

  ObserverMap observers_;
  raw_ptr<PrefService, AcrossTasksDanglingUntriaged> service_;
};

#endif  // COMPONENTS_PREFS_PREF_CHANGE_REGISTRAR_H_
