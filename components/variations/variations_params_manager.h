// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_PARAMS_MANAGER_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_PARAMS_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/test/scoped_field_trial_list_resetter.h"

namespace base {
class CommandLine;
class FieldTrialList;

namespace test {
class ScopedFeatureList;
}  // namespace test

}  // namespace base

namespace variations {
namespace testing {

// NOTE: THIS CLASS IS DEPRECATED. Please use ScopedFeatureList instead, which
// provides equivalent functionality.
// TODO(asvitkine): Migrate callers and remove this class.
//
// Use this class as a member in your test class to set variation params for
// your tests. You can directly set the parameters in the constructor (if they
// are used by other members upon construction). You can change them later
// arbitrarily many times using the SetVariationParams function. Internally, it
// creates a FieldTrialList as a member. It works well for multiple tests of a
// given test class, as it clears the parameters when this class is destructed.
// Note that it clears all parameters (not just those registered here).
class VariationParamsManager {
 public:
  // Does not associate any parameters.
  VariationParamsManager();
  // Calls directly SetVariationParams with the provided arguments.
  VariationParamsManager(
      const std::string& trial_name,
      const std::map<std::string, std::string>& param_values);
  // Calls directly SetVariationParamsWithFeatures with the provided arguments.
  VariationParamsManager(
      const std::string& trial_name,
      const std::map<std::string, std::string>& param_values,
      const std::set<std::string>& associated_features);
  ~VariationParamsManager();

  // Associates |param_values| with the given |trial_name|. |param_values| maps
  // parameter names to their values. The function creates a new trial group,
  // used only for testing. Between two calls of this function,
  // ClearAllVariationParams() has to be called.
  void SetVariationParams(
      const std::string& trial_name,
      const std::map<std::string, std::string>& param_values);

  // Like SetVariationParams(). |associated_features| lists names of features
  // to be associated to the newly created trial group. As a result, all
  // parameters from |param_values| can be accessed via any of the feature from
  // |associated_features|.
  void SetVariationParamsWithFeatureAssociations(
      const std::string& trial_name,
      const std::map<std::string, std::string>& param_values,
      const std::set<std::string>& associated_features);

  // Clears all of the mapped associations.
  void ClearAllVariationIDs();

  // Clears all of the associated params.
  void ClearAllVariationParams();

  // Appends command line switches to |command_line| in a way that mimics
  // SetVariationParams.
  //
  // This static method is useful in situations where using
  // VariationParamsManager directly would have resulted in initializing
  // FieldTrialList twice (once from ChromeBrowserMainParts::SetupFieldTrials
  // and once from VariationParamsManager).
  static void AppendVariationParams(
      const std::string& trial_name,
      const std::string& trial_group_name,
      const std::map<std::string, std::string>& param_values,
      base::CommandLine* command_line);

 private:
  base::test::ScopedFieldTrialListResetter field_trial_list_resetter_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(VariationParamsManager);
};

}  // namespace testing
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_PARAMS_MANAGER_H_
