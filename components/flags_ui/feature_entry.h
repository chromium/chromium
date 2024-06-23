// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_FEATURE_ENTRY_H_
#define COMPONENTS_FLAGS_UI_FEATURE_ENTRY_H_

#include <string>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/chromeos_buildflags.h"

namespace flags_ui {

extern const char kMultiSeparatorChar;

// Generic experiment choice option names.
extern const char kGenericExperimentChoiceDefault[];
extern const char kGenericExperimentChoiceEnabled[];
extern const char kGenericExperimentChoiceDisabled[];
extern const char kGenericExperimentChoiceAutomatic[];

// FeatureEntry is used to describe an experimental feature.
//
// Note that features should eventually be either turned on by default with no
// about_flags entries or deleted. Most feature entries should only be around
// for a few milestones, until their full launch.
struct FeatureEntry {
  enum Type : unsigned short {
    // A feature with a single flag value.
    //
    // For new entries, it is recommended to instead use FEATURE_VALUE macro
    // that is backed by a base::Feature struct. See base/feature_list.h.
    SINGLE_VALUE,

    // A default enabled feature with a single flag value to disable it.
    //
    // For new entries, it is recommended to instead use FEATURE_VALUE macro
    // that is backed by a base::Feature struct. See base/feature_list.h.
    SINGLE_DISABLE_VALUE,

    // The feature has multiple values only one of which is ever enabled.
    // The first of the values should correspond to a deactivated state for this
    // feature (i.e. no command line option). For MULTI_VALUE entries, the
    // command_line of the FeatureEntry is not used. If the experiment is
    // enabled the command line of the selected Choice is enabled.
    MULTI_VALUE,

    // The feature has three possible values: Default, Enabled and Disabled.
    // This allows the Default group to have its own logic to determine if the
    // feature is on.
    //
    // For new entries, it is recommended to instead use FEATURE_VALUE macro
    // that is backed by a base::Feature struct. See base/feature_list.h.
    ENABLE_DISABLE_VALUE,

    // Corresponds to a base::Feature, per base/feature_list.h. The entry will
    // have three states: Default, Enabled, Disabled. When not specified or set
    // to Default, the normal default value of the feature is used.
    //
    // This is recommended for all new entries, since it provides a uniform way
    // to specify features in the codebase along with their default state, as
    // well as the ability enable/disable via run server-side experiments.
    FEATURE_VALUE,

    // Corresponds to a base::Feature and additional options [O_1, ..., O_n]
    // that specify field trial params. Each of the options can specify a set
    // of field trial params. The entry will have n+3 states: Default, Enabled,
    // Enabled V_1, ..., Enabled: V_n, Disabled. When set to Default, the normal
    // default values of the feature and of the parameters are used (possibly
    // passed from the server in a trial config). When set to Enabled, the
    // feature is overriden to be enabled and empty set of parameters is used
    // boiling down to the default behavior in the code.
    FEATURE_WITH_PARAMS_VALUE,

    // Corresponds to a command line switch where the value is treated as a list
    // of url::Origins. (Lists will not be reordered.) Default state is
    // disabled like SINGLE_VALUE.
    ORIGIN_LIST_VALUE,

    // Corresponds to a command line switch where the value is an arbitrary
    // string. Default state is disabled like SINGLE_VALUE.
    STRING_VALUE,

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // The below two types are for *platform* features -- that is, those defined
    // and queried via platform2/featured/feature_library.h. Such features
    // should be defined outside of the browser (e.g., in platform2 or
    // platform) using a compile-time-constant default value and name.
    // See feature_library.h for more documentation.

    // Corresponds to a feature *name*, starting with "CrOSLateBoot", for a
    // platform feature.
    //
    // Broadly similar to FEATURE_VALUE, but we cannot define |base::Feature|s
    // starting with CrOSLateBoot in the browser directly -- they must instead
    // be defined and queried outside of the browser, using
    // platform2/featured/feature_library.h.
    PLATFORM_FEATURE_NAME_VALUE,

    // Corresponds to a feature *name*, starting with "CrOSLateBoot", for a
    // platform feature, along with its parameters.
    //
    // Broadly similar to FEATURE_WITH_PARAMS_VALUE, but we cannot define
    // |base::Feature|s starting with CrOSLateBoot in the browser directly --
    // they must instead be defined and queried outside of the browser, using
    // platform2/featured/feature_library.h.
    PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  };

  // Describes state of a feature.
  enum FeatureState {
    // The state of the feature is not overridden by the user.
    DEFAULT,
    // The feature is enabled by the user.
    ENABLED,
    // The feature is disabled by the user.
    DISABLED,
  };

  // Used for MULTI_VALUE types to describe one of the possible values the user
  // can select.
  struct Choice {
    // The message containing the choice name.
    const char* description;

    // Command line switch and value to enabled for this choice.
    const char* command_line_switch;
    // Simple switches that have no value should use "" for command_line_value.
    const char* command_line_value;
  };

  // Configures one parameter for FEATURE_WITH_PARAMS_VALUE.
  struct FeatureParam {
    const char* param_name;
    const char* param_value;
  };

  // Specified one variation (list of parameter values) for
  // FEATURE_WITH_PARAMS_VALUE.
  struct FeatureVariation {
    // Text that denotes the variation in chrome://flags. For each variation,
    // the user is shown an option labeled "Enabled <description_text>" (with
    // the exception of the first option labeled "Enabled" to make clear it is
    // the default one). No need for description_id, chrome://flags should not
    // get translated. The other parts here use ids for historical reasons and
    // can realistically also be moved to direct description_texts.
    const char* description_text;
    // RAW_PTR_EXCLUSION: #global-scope
    RAW_PTR_EXCLUSION const FeatureParam* params;
    int num_params;
    // A variation id number in the format of
    // VariationsIdsProvider::ForceVariationIds() or nullptr if you do
    // not need to set any variation_id for this feature variation.
    const char* variation_id;
  };

  // The internal name of the feature entry. This is never shown to the user.
  // It _is_ however stored in the prefs file, so you shouldn't change the
  // name of existing flags.
  const char* internal_name;

  // The feature's name.
  const char* visible_name;

  // The feature's description.
  const char* visible_description;

  // The platforms the feature is available on.
  // Needs to be more than a compile-time #ifdef because of profile sync.
  unsigned short supported_platforms;  // bitmask

  // Type of entry.
  Type type;

  union {
    struct {
      // The commandline switch and value that are added when this flag is
      // active. This is different from |internal_name| so that the commandline
      // flag can be renamed without breaking the prefs file. This is used if
      // type is SINGLE_VALUE or ENABLE_DISABLE_VALUE.
      const char* command_line_switch;

      // Simple switches that have no value should use "" for
      // command_line_value.
      const char* command_line_value;

      // For ENABLE_DISABLE_VALUE, the command line switch and value to
      // explicitly disable the feature.
      const char* disable_command_line_switch;
      const char* disable_command_line_value;
    } switches;

    struct {
      // For FEATURE_VALUE or FEATURE_WITH_PARAMS_VALUE, the base::Feature
      // this entry corresponds to. The same feature must not be used in
      // multiple FeatureEntries.
      // This field is not a raw_ptr<> because it was filtered by the rewriter
      // for: #union, #global-scope
      RAW_PTR_EXCLUSION const base::Feature* feature;

      // This describes the options if type is FEATURE_WITH_PARAMS_VALUE.
      // The first variation is the default "Enabled" variation, its
      // description_id is disregarded.
      base::span<const FeatureVariation> feature_variations;

      // The name of the FieldTrial in which the selected variation parameters
      // should be registered. This is used if type is
      // FEATURE_WITH_PARAMS_VALUE.
      const char* feature_trial_name;
    } feature;

    struct {
      // For PLATFORM_FEATURE_NAME_TYPE or
      // PLATFORM_FEATURE_WITH_PARAMS_VALUE_TYPE, the name of the feature this
      // entry corresponds to. The same feature must not be used in multiple
      // FeatureEntries.
      const char* name;

      // This describes the options if type is
      // PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE.
      // The first variation is the default "Enabled" variation, its
      // description_id is disregarded.
      base::span<const FeatureVariation> feature_variations;

      // The name of the FieldTrial in which the selected variation parameters
      // should be registered. This is used if type is
      // PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE.
      const char* feature_trial_name;
    } platform_feature_name;

    // This describes the options if type is MULTI_VALUE.
    base::span<const Choice> choices;
  };

  // This describes the links to be rendered as <a> in the chrome://flags
  // page.
  base::span<const char* const> links;

  // Check whether internal |name| matches this FeatureEntry. Depending on the
  // type of entry, this compared it to either |internal_name| or the values
  // produced by NameForOption().
  bool InternalNameMatches(const std::string& name) const;

  // Number of options to choose from. This is used if type is MULTI_VALUE,
  // ENABLE_DISABLE_VALUE, FEATURE_VALUE, or FEATURE_WITH_PARAMS_VALUE.
  int NumOptions() const;

  // Returns the name used in prefs for the option at the specified |index|.
  // Only used for types that use |num_options|.
  std::string NameForOption(int index) const;

  // Returns the human readable description for the option at |index|.
  // Only used for types that use |num_options|.
  std::u16string DescriptionForOption(int index) const;

  // Returns the choice for the option at |index|. Only applicable for type
  // FEATURE_MULTI.
  const FeatureEntry::Choice& ChoiceForOption(int index) const;

  // Returns the state of the feature at |index|. Only applicable for types
  // FEATURE_VALUE and FEATURE_WITH_PARAMS_VALUE.
  FeatureEntry::FeatureState StateForOption(int index) const;

  // Returns the variation for the option at |index| or nullptr if there is no
  // variation associated at |index|. Only applicable for types FEATURE_VALUE
  // and FEATURE_WITH_PARAMS_VALUE.
  const FeatureEntry::FeatureVariation* VariationForOption(int index) const;

  // Returns true if the entry is considered as valid.
  // See the implenetation for the details of what is valid.
  bool IsValid() const;

  // Returns the variation list. Type must be either
  // FEATURE_WITH_PARAMS_VALUE or PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE.
  const base::span<const FeatureVariation> GetVariations() const;
};

namespace testing {

// Separator used for multi values. Multi values are represented in prefs as
// name-of-experiment + kMultiSeparator + selected_index.
extern const char kMultiSeparator[];

}  // namespace testing

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_FEATURE_ENTRY_H_
