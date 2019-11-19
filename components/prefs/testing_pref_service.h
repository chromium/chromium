// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_TESTING_PREF_SERVICE_H_
#define COMPONENTS_PREFS_TESTING_PREF_SERVICE_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"

class PrefNotifierImpl;
class PrefRegistrySimple;
class TestingPrefStore;

// A PrefService subclass for testing. It operates totally in memory and
// provides additional API for manipulating preferences at the different levels
// (managed, extension, user) conveniently.
//
// Use this via its specializations, e.g. TestingPrefServiceSimple.
template <class SuperPrefService, class ConstructionPrefRegistry>
class TestingPrefServiceBase : public SuperPrefService {
 public:
  virtual ~TestingPrefServiceBase();

  // Reads the value of a preference from the managed layer. Returns NULL if the
  // preference is not defined at the managed layer.
  const base::Value* GetManagedPref(const std::string& path) const;

  // Sets a preference on the managed layer and fires observers if the
  // preference changed.
  void SetManagedPref(const std::string& path,
                      std::unique_ptr<base::Value> value);

  // Clears the preference on the managed layer and fire observers if the
  // preference has been defined previously.
  void RemoveManagedPref(const std::string& path);

  // Similar to the above, but for extension preferences.
  // Does not really know about extensions and their order of installation.
  // Useful in tests that only check that a preference is overridden by an
  // extension.
  const base::Value* GetExtensionPref(const std::string& path) const;
  void SetExtensionPref(const std::string& path,
                        std::unique_ptr<base::Value> value);
  void RemoveExtensionPref(const std::string& path);

  // Similar to the above, but for user preferences.
  const base::Value* GetUserPref(const std::string& path) const;
  void SetUserPref(const std::string& path, std::unique_ptr<base::Value> value);
  void RemoveUserPref(const std::string& path);

  // Similar to the above, but for recommended policy preferences.
  const base::Value* GetRecommendedPref(const std::string& path) const;
  void SetRecommendedPref(const std::string& path,
                          std::unique_ptr<base::Value> value);
  void RemoveRecommendedPref(const std::string& path);

  // Do-nothing implementation for TestingPrefService.
  static void HandleReadError(PersistentPrefStore::PrefReadError error) {}

  // Set initialization status of pref stores.
  void SetInitializationCompleted();

 protected:
  TestingPrefServiceBase(TestingPrefStore* managed_prefs,
                         TestingPrefStore* extension_prefs,
                         TestingPrefStore* user_prefs,
                         TestingPrefStore* recommended_prefs,
                         ConstructionPrefRegistry* pref_registry,
                         PrefNotifierImpl* pref_notifier);

 private:
  // Reads the value of the preference indicated by |path| from |pref_store|.
  // Returns NULL if the preference was not found.
  const base::Value* GetPref(TestingPrefStore* pref_store,
                             const std::string& path) const;

  // Sets the value for |path| in |pref_store|.
  void SetPref(TestingPrefStore* pref_store,
               const std::string& path,
               std::unique_ptr<base::Value> value);

  // Removes the preference identified by |path| from |pref_store|.
  void RemovePref(TestingPrefStore* pref_store, const std::string& path);

  // Pointers to the pref stores our value store uses.
  scoped_refptr<TestingPrefStore> managed_prefs_;
  scoped_refptr<TestingPrefStore> extension_prefs_;
  scoped_refptr<TestingPrefStore> user_prefs_;
  scoped_refptr<TestingPrefStore> recommended_prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceBase);
};

// Test version of PrefService.
class TestingPrefServiceSimple
    : public TestingPrefServiceBase<PrefService, PrefRegistry> {
 public:
  TestingPrefServiceSimple();
  ~TestingPrefServiceSimple() override;

  // This is provided as a convenience for registering preferences on
  // an existing TestingPrefServiceSimple instance. On a production
  // PrefService you would do all registrations before constructing
  // it, passing it a PrefRegistry via its constructor (or via
  // e.g. PrefServiceFactory).
  PrefRegistrySimple* registry();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceSimple);
};

template<>
TestingPrefServiceBase<PrefService, PrefRegistry>::TestingPrefServiceBase(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* extension_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    PrefRegistry* pref_registry,
    PrefNotifierImpl* pref_notifier);

template<class SuperPrefService, class ConstructionPrefRegistry>
TestingPrefServiceBase<
    SuperPrefService, ConstructionPrefRegistry>::~TestingPrefServiceBase() {
}

template <class SuperPrefService, class ConstructionPrefRegistry>
const base::Value* TestingPrefServiceBase<
    SuperPrefService,
    ConstructionPrefRegistry>::GetManagedPref(const std::string& path) const {
  return GetPref(managed_prefs_.get(), path);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    SetManagedPref(const std::string& path,
                   std::unique_ptr<base::Value> value) {
  SetPref(managed_prefs_.get(), path, std::move(value));
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    RemoveManagedPref(const std::string& path) {
  RemovePref(managed_prefs_.get(), path);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
const base::Value* TestingPrefServiceBase<
    SuperPrefService,
    ConstructionPrefRegistry>::GetExtensionPref(const std::string& path) const {
  return GetPref(extension_prefs_.get(), path);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    SetExtensionPref(const std::string& path,
                     std::unique_ptr<base::Value> value) {
  SetPref(extension_prefs_.get(), path, std::move(value));
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    RemoveExtensionPref(const std::string& path) {
  RemovePref(extension_prefs_.get(), path);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
const base::Value*
TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::GetUserPref(
    const std::string& path) const {
  return GetPref(user_prefs_.get(), path);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    SetUserPref(const std::string& path, std::unique_ptr<base::Value> value) {
  SetPref(user_prefs_.get(), path, std::move(value));
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    RemoveUserPref(const std::string& path) {
  RemovePref(user_prefs_.get(), path);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
const base::Value*
TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    GetRecommendedPref(const std::string& path) const {
  return GetPref(recommended_prefs_, path);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    SetRecommendedPref(const std::string& path,
                       std::unique_ptr<base::Value> value) {
  SetPref(recommended_prefs_.get(), path, std::move(value));
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    RemoveRecommendedPref(const std::string& path) {
  RemovePref(recommended_prefs_.get(), path);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
const base::Value*
TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::GetPref(
    TestingPrefStore* pref_store,
    const std::string& path) const {
  const base::Value* res;
  return pref_store->GetValue(path, &res) ? res : NULL;
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    SetPref(TestingPrefStore* pref_store,
            const std::string& path,
            std::unique_ptr<base::Value> value) {
  pref_store->SetValue(path, std::move(value),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    RemovePref(TestingPrefStore* pref_store, const std::string& path) {
  pref_store->RemoveValue(path, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

template <class SuperPrefService, class ConstructionPrefRegistry>
void TestingPrefServiceBase<SuperPrefService, ConstructionPrefRegistry>::
    SetInitializationCompleted() {
  managed_prefs_->SetInitializationCompleted();
  extension_prefs_->SetInitializationCompleted();
  recommended_prefs_->SetInitializationCompleted();
  // |user_prefs_| is initialized in PrefService constructor so no need to
  // set initialization status again.
}

#endif  // COMPONENTS_PREFS_TESTING_PREF_SERVICE_H_
