// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/flags_ui/feature_entry.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace flags_ui {

// WARNING: '@' is also used in the html file. If you update this constant you
// also need to update the html file.
const char kMultiSeparatorChar = '@';

// These descriptions are translated for display in Chrome Labs. If these
// strings are changed the translated strings in Chrome Labs must also be
// changed (IDS_CHROMELABS_XXX).
const char kGenericExperimentChoiceDefault[] = "Default";
const char kGenericExperimentChoiceEnabled[] = "Enabled";
const char kGenericExperimentChoiceDisabled[] = "Disabled";
const char kGenericExperimentChoiceAutomatic[] = "Automatic";

bool FeatureEntry::InternalNameMatches(const std::string& name) const {
  if (!base::StartsWith(name, internal_name, base::CompareCase::SENSITIVE))
    return false;

  const size_t internal_name_length = strlen(internal_name);
  switch (type) {
    case FeatureEntry::SINGLE_VALUE:
    case FeatureEntry::SINGLE_DISABLE_VALUE:
    case FeatureEntry::ORIGIN_LIST_VALUE:
    case FeatureEntry::STRING_VALUE:
      return name.size() == internal_name_length;

    case FeatureEntry::MULTI_VALUE:
    case FeatureEntry::ENABLE_DISABLE_VALUE:
    case FeatureEntry::FEATURE_VALUE:
    case FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case FeatureEntry::PLATFORM_FEATURE_NAME_VALUE:
    case FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE:
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      // Check that the pattern matches what's produced by NameForOption().
      int index = -1;
      return name.size() > internal_name_length + 1 &&
             name[internal_name_length] == kMultiSeparatorChar &&
             base::StringToInt(name.substr(internal_name_length + 1), &index) &&
             index >= 0 && index < NumOptions();
  }
}

int FeatureEntry::NumOptions() const {
  switch (type) {
    case ENABLE_DISABLE_VALUE:
    case FEATURE_VALUE:
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case PLATFORM_FEATURE_NAME_VALUE:
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      return 3;
    case MULTI_VALUE:
      return choices.size();
    case FEATURE_WITH_PARAMS_VALUE:
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE:
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      return 3 + GetVariations().size();
    default:
      return 0;
  }
}

std::string FeatureEntry::NameForOption(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE
#if BUILDFLAG(IS_CHROMEOS_ASH)
         || type == FeatureEntry::PLATFORM_FEATURE_NAME_VALUE ||
         type == FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  );
  DCHECK_LT(index, NumOptions());
  return std::string(internal_name) + testing::kMultiSeparator +
         base::NumberToString(index);
}

// The order in which these descriptions are returned is the same in the
// LabsComboboxModel::GetItemAt(..) (in the chrome_labs_item_view.cc file) for
// the translated version of these strings. If there are changes to this, the
// same changes must be made in LabsComboboxModel
std::u16string FeatureEntry::DescriptionForOption(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE
#if BUILDFLAG(IS_CHROMEOS_ASH)
         || type == FeatureEntry::PLATFORM_FEATURE_NAME_VALUE ||
         type == FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  );
  DCHECK_LT(index, NumOptions());
  const char* description = nullptr;
  if (type == FeatureEntry::ENABLE_DISABLE_VALUE ||
      type == FeatureEntry::FEATURE_VALUE
#if BUILDFLAG(IS_CHROMEOS_ASH)
      || type == FeatureEntry::PLATFORM_FEATURE_NAME_VALUE
#endif
  ) {
    const char* const kEnableDisableDescriptions[] = {
        kGenericExperimentChoiceDefault,
        kGenericExperimentChoiceEnabled,
        kGenericExperimentChoiceDisabled,
    };
    description = kEnableDisableDescriptions[index];
  } else if (type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE
#if BUILDFLAG(IS_CHROMEOS_ASH)
             || type == FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE
#endif
  ) {
    if (index == 0) {
      description = kGenericExperimentChoiceDefault;
    } else if (index == 1) {
      description = kGenericExperimentChoiceEnabled;
    } else if (index < NumOptions() - 1) {
      // First two options do not have variations params.
      int variation_index = index - 2;
      return base::ASCIIToUTF16(
          base::StrCat({kGenericExperimentChoiceEnabled, " ",
                        GetVariations()[variation_index].description_text}));
    } else {
      DCHECK_EQ(NumOptions() - 1, index);
      description = kGenericExperimentChoiceDisabled;
    }
  } else {
    description = choices[index].description;
  }
  return base::ASCIIToUTF16(description);
}

const FeatureEntry::Choice& FeatureEntry::ChoiceForOption(int index) const {
  DCHECK_EQ(FeatureEntry::MULTI_VALUE, type);
  DCHECK_LT(index, NumOptions());

  return choices[index];
}

FeatureEntry::FeatureState FeatureEntry::StateForOption(int index) const {
  DCHECK(type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE
#if BUILDFLAG(IS_CHROMEOS_ASH)
         || type == FeatureEntry::PLATFORM_FEATURE_NAME_VALUE ||
         type == FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE
#endif
  );
  DCHECK_LT(index, NumOptions());

  if (index == 0)
    return FeatureEntry::FeatureState::DEFAULT;
  if (index == NumOptions() - 1)
    return FeatureEntry::FeatureState::DISABLED;
  return FeatureEntry::FeatureState::ENABLED;
}

const FeatureEntry::FeatureVariation* FeatureEntry::VariationForOption(
    int index) const {
  DCHECK(type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE
#if BUILDFLAG(IS_CHROMEOS_ASH)
         || type == FeatureEntry::PLATFORM_FEATURE_NAME_VALUE ||
         type == FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE
#endif
  );
  DCHECK_LT(index, NumOptions());

  if ((type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE
#if BUILDFLAG(IS_CHROMEOS_ASH)
       || type == FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE
#endif
       ) &&
      index > 1 && index < NumOptions() - 1) {
    // We have no variations for FEATURE_VALUE type or
    // PLATFORM_FEATURE_NAME_VALUE type. Option at |index| corresponds to
    // variation at |index| - 2 as the list starts with "Default" and "Enabled"
    // (with default parameters).
    return &GetVariations()[index - 2];
  }

  return nullptr;
}

bool FeatureEntry::IsValid() const {
  switch (type) {
    case FeatureEntry::SINGLE_VALUE:
    case FeatureEntry::SINGLE_DISABLE_VALUE:
    case FeatureEntry::ORIGIN_LIST_VALUE:
    case FeatureEntry::STRING_VALUE:
      return true;
    case FeatureEntry::MULTI_VALUE:
      if (choices.size() == 0) {
        LOG(ERROR) << "no choice is found";
        return false;
      }
      if (!ChoiceForOption(0).command_line_switch) {
        LOG(ERROR) << "command_line_swtich is null";
        return false;
      }
      if (ChoiceForOption(0).command_line_switch[0] != '\0') {
        LOG(ERROR) << "The command line value of the first item must be empty";
        return false;
      }
      return true;
    case FeatureEntry::ENABLE_DISABLE_VALUE:
      if (!switches.command_line_switch) {
        LOG(ERROR) << "command_line_switch is null";
        return false;
      }
      if (!switches.command_line_value) {
        LOG(ERROR) << "command_line_value is null";
        return false;
      }
      if (!switches.disable_command_line_switch) {
        LOG(ERROR) << "disable_command_line_switch is null";
        return false;
      }
      if (!switches.disable_command_line_value) {
        LOG(ERROR) << "disable_command_line_value is null";
        return false;
      }
      return true;
    case FeatureEntry::FEATURE_VALUE:
      if (!feature.feature) {
        LOG(ERROR) << "no feature is set";
        return false;
      }
      return true;
    case FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
      if (!feature.feature) {
        LOG(ERROR) << "no feature is set";
        return false;
      }
      if (feature.feature_variations.size() == 0) {
        LOG(ERROR) << "feature_variations is empty";
        return false;
      }
      if (!feature.feature_trial_name) {
        LOG(ERROR) << "feature_trial_name is null";
        return false;
      }
      return true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case FeatureEntry::PLATFORM_FEATURE_NAME_VALUE:
      if (!platform_feature_name.name) {
        LOG(ERROR) << "no feature name is set";
        return false;
      }
#if BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX)
      if (!base::StartsWith(platform_feature_name.name,
                            BUILDFLAG(BANNED_BASE_FEATURE_PREFIX))) {
        LOG(ERROR) << "missing required feature name prefix, please check "
                      "BANNED_BASE_FEATURE_PREFIX";
        return false;
      }
#endif  // BUILDFLAG(ENABLED_BANNED_BASE_FEATURE_PREFIX)
      return true;
    case FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE:
      if (!platform_feature_name.name) {
        LOG(ERROR) << "no feature name is set";
        return false;
      }
      if (platform_feature_name.feature_variations.size() == 0) {
        LOG(ERROR) << "feature_variations is empty";
        return false;
      }
      if (!platform_feature_name.feature_trial_name) {
        LOG(ERROR) << "feature_trial_name is null";
        return false;
      }
#if BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX)
      if (!base::StartsWith(platform_feature_name.name,
                            BUILDFLAG(BANNED_BASE_FEATURE_PREFIX))) {
        LOG(ERROR) << "missing required feature name prefix, please check "
                      "BANNED_BASE_FEATURE_PREFIX";
        return false;
      }
#endif  // BUILDFLAG(ENABLED_BANNED_BASE_FEATURE_PREFIX)
      return true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

const base::span<const FeatureEntry::FeatureVariation>
FeatureEntry::GetVariations() const {
  if (type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE) {
    return feature.feature_variations;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (type == FeatureEntry::PLATFORM_FEATURE_NAME_WITH_PARAMS_VALUE) {
    return platform_feature_name.feature_variations;
  }
#endif
  NOTREACHED_IN_MIGRATION();
  return base::span<const FeatureEntry::FeatureVariation>();
}

namespace testing {

const char kMultiSeparator[] = {kMultiSeparatorChar, '\0'};

}  // namespace testing

}  // namespace flags_ui
