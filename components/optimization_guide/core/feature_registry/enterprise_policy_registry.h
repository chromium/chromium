// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_ENTERPRISE_POLICY_REGISTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_ENTERPRISE_POLICY_REGISTRY_H_

#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"

class PrefService;

namespace optimization_guide {

// EnterprisePolicyPref holds configuration for an enterprise policy controlling
// a model execution feature.
class EnterprisePolicyPref {
 public:
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  explicit EnterprisePolicyPref(const char* name);

  const char* name() const { return name_; }

  // Returns the current setting of the enterprise policy.
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  model_execution::prefs::ModelExecutionEnterprisePolicyValue GetValue(
      const PrefService*) const;

 private:
  // The full pref path controlling the enterprise policy.
  const char* name_;
};

class EnterprisePolicyRegistry {
 public:
  EnterprisePolicyRegistry();
  ~EnterprisePolicyRegistry();

  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  static EnterprisePolicyRegistry& GetInstance();

  // Registers an enterprise policy pref. Features should register themselves in
  // components/optimization_guide/core/feature_registry/feature_registration.cc.
  // Note that this does not cause the pref to be immediately registered in the
  // pref service: RegisterProfilePrefs must be called to do so (only once all
  // features are done registering their enterprise policy via Register calls).
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  EnterprisePolicyPref Register(const char* name);

  // Registers all the prefs this registry holds into the pref registry.
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  void RegisterProfilePrefs(PrefRegistrySimple* registry);

  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  void ClearForTesting();

 private:
  std::vector<EnterprisePolicyPref> enterprise_policies_;

  // Set to true once RegisterProfilePrefs is called. At that point it's too
  // late to call Register. This is used to CHECK Register isn't called too
  // late.
  bool immutable_ = false;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_ENTERPRISE_POLICY_REGISTRY_H_
