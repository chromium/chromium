// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"

#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_quality/addresses/address_normalizer.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

namespace {

// Represents a sequence of consecutive integers.
// This is not a `base::span` because we need the indices for different
// containers.
struct Sequence {
  size_t offset = 0;
  size_t length = 0;
  bool reverse = false;
};

// Returns a longest sequence of non-negative consecutive integers in `nums`.
// E.g., returns {.offset = 3, .length = 3} for {-1, 1, 2, 3, 5, 1, 2, 3}.
Sequence GetLongestConsecutiveNonNegativeSequence(base::span<const int> nums) {
  Sequence longest;
  Sequence current;
  for (size_t i = 0; i < nums.size(); ++i) {
    if (nums[i] < 0) {
      continue;
    }
    if (i == 0 || nums[i - 1] < 0 || nums[i - 1] + 1 != nums[i]) {
      if (longest.length < current.length) {
        longest = current;
      }
      current = {.offset = i, .length = 1};
    } else {
      ++current.length;
    }
  }
  if (longest.length < current.length) {
    longest = current;
  }
  return longest;
}

// Removes the non-digit prefix and suffix. For example,
// `TrimNonDigits("foo12x3bar")` returns "12x3".
std::u16string_view TrimNonDigits(std::u16string_view s) {
  // There's no need to decode the Unicode characters: If `s.front()` is a
  // second code unit ("low surrogate"), its value does not overlap with ASCII.
  while (!s.empty() && !base::IsAsciiDigit(s.front())) {
    s = s.substr(1);
  }
  while (!s.empty() && !base::IsAsciiDigit(s.back())) {
    s = s.substr(0, s.size() - 1);
  }
  return s;
}

// Returns a vector of the same size as `option` where the `i`th integer is the
// non-negative integer found in `option` or -1 if there is none.
std::vector<int> ExtractNumbers(base::span<const SelectOption> options,
                                std::u16string SelectOption::* selector) {
  return base::ToVector(options, [&selector](const SelectOption& option) {
    int num;
    if (base::StringToInt(TrimNonDigits(option.*selector), &num) && num >= 0) {
      return num;
    }
    return -1;
  });
}

// Returns the SelectOption::value of `field.options()` that matches
// `attribute`'s day.
std::optional<std::u16string> GetDaySelectControlValue(
    const AttributeInstance& attribute,
    const AutofillField& field,
    const std::string& app_locale) {
  // Months have 28 to 31 days. We tolerate two additional options (e.g.,
  // "Pick day" and "Unknown").
  if (field.options().size() < 28 || field.options().size() > 33) {
    return std::nullopt;
  }

  std::u16string day_string = attribute.GetInfo(
      field.Type().GetStorableType(), app_locale, std::u16string(u"D"));
  int day = 0;
  if (!base::StringToInt(day_string, &day) || day < 1 || day > 31) {
    return std::nullopt;
  }

  int index = [&]() -> int {
    std::vector<int> text_nums =
        ExtractNumbers(field.options(), &SelectOption::text);
    std::vector<int> value_nums =
        ExtractNumbers(field.options(), &SelectOption::value);

    Sequence text_seq = GetLongestConsecutiveNonNegativeSequence(text_nums);
    Sequence value_seq = GetLongestConsecutiveNonNegativeSequence(value_nums);

    // The user-visible text must start at "1".
    if (28 <= text_seq.length && text_seq.length <= 31 &&
        text_nums[text_seq.offset] == 1) {
      return text_seq.offset + day - 1;
    }
    // The user-invisible values must start at "0" or "1".
    if (28 <= value_seq.length && value_seq.length <= 31 &&
        text_nums[text_seq.offset] <= 1) {
      return value_seq.offset + day - 1;
    }
    return -1;
  }();

  if (index < 0 || static_cast<size_t>(index) >= field.options().size()) {
    return std::nullopt;
  }
  return field.options()[index].value;
}

// Returns the SelectOption::value of `field.options()` that matches
// `attribute`'s month.
std::optional<std::u16string> GetMonthSelectControlValue(
    const AttributeInstance& attribute,
    const AutofillField& field,
    const std::string& app_locale) {
  // There are 12 months. We tolerate two additional options (e.g.,
  // "Pick month" and "Unknown").
  if (field.options().size() < 12 || field.options().size() > 14) {
    return std::nullopt;
  }

  std::u16string month_string = attribute.GetInfo(
      field.Type().GetStorableType(), app_locale, std::u16string(u"M"));
  int month = 0;
  if (!base::StringToInt(month_string, &month) || month < 1 || month > 12) {
    return std::nullopt;
  }

  int index = [&]() -> int {
    std::vector<int> text_nums =
        ExtractNumbers(field.options(), &SelectOption::text);
    std::vector<int> value_nums =
        ExtractNumbers(field.options(), &SelectOption::value);

    Sequence text_seq = GetLongestConsecutiveNonNegativeSequence(text_nums);
    Sequence value_seq = GetLongestConsecutiveNonNegativeSequence(value_nums);

    // The user-visible text must contain "1" to "12".
    if (text_seq.length == 12 && text_nums[text_seq.offset] == 1) {
      return text_seq.offset + month - 1;
    }
    // The user-invisible values must be "1" to "12" or "0" to "11".
    if (value_seq.length == 12 && text_nums[text_seq.offset] <= 1) {
      return value_seq.offset + month - 1;
    }
    // If there are no numbers, perhaps the months are spelled out.
    if (text_seq.length == 0 && value_seq.length == 0 &&
        field.options().size() == 12) {
      return month - 1;
    }
    return -1;
  }();

  if (index < 0) {
    return std::nullopt;
  }
  return field.options()[index].value;
}

// Returns the SelectOption::value of `field.options()` that matches
// `attribute`'s year.
std::optional<std::u16string> GetYearSelectControlValue(
    const AttributeInstance& attribute,
    const AutofillField& field,
    const std::string& app_locale) {
  std::u16string year_string = attribute.GetInfo(
      field.Type().GetStorableType(), app_locale, std::u16string(u"YYYY"));
  int yyyy = 0;
  if (!base::StringToInt(year_string, &yyyy)) {
    return std::nullopt;
  }

  auto find_year = [&](base::span<const int> nums) {
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (seq.length < 2) {
      return -1;
    }

    // Returns the index of `target` in `nums` if it occurs within `seq`.
    // Returns -1 otherwise.
    auto find_in_seq = [&nums, &seq](int target) -> int {
      size_t index = seq.offset + target - nums[seq.offset];
      if (index < 0 || index >= seq.length || target != nums[index]) {
        return -1;
      }
      return index;
    };

    // Search for the four-digit year.
    if (int index = find_in_seq(yyyy); index >= 0) {
      return index;
    }

    // Search for the two-digit year, with an extra plausibility check to avoid
    // confusion with day or month fields.
    int yy = yyyy % 100;
    bool may_be_day_or_month = (yy <= 12 && seq.length == 12) ||
                               (28 <= seq.length && seq.length <= 31);
    if (int index = find_in_seq(yy); index >= 0 && !may_be_day_or_month) {
      return index;
    }

    return -1;
  };

  int index = find_year(ExtractNumbers(field.options(), &SelectOption::text));
  if (index < 0) {
    index = find_year(ExtractNumbers(field.options(), &SelectOption::value));
  }

  if (index < 0) {
    return std::nullopt;
  }
  return field.options()[index].value;
}

// Looks for the day, month, or year from `attribute` to fill into `field`.
std::optional<std::u16string> GetValueForDateSelectControl(
    const AttributeInstance& attribute,
    const AutofillField& field,
    const std::string& app_locale) {
  FieldType type = field.Type().GetStorableType();
  if (!IsDateFieldType(type)) {
    return std::nullopt;
  }
  if (std::optional<std::u16string> match =
          GetDaySelectControlValue(attribute, field, app_locale)) {
    return match;
  }
  if (std::optional<std::u16string> match =
          GetMonthSelectControlValue(attribute, field, app_locale)) {
    return match;
  }
  if (std::optional<std::u16string> match =
          GetYearSelectControlValue(attribute, field, app_locale)) {
    return match;
  }
  return std::nullopt;
}

std::u16string GetValueForInput(const AttributeInstance& attribute,
                                const AutofillField& field,
                                const std::string& app_locale) {
  FieldType type = field.Type().GetStorableType();
  // TODO(crbug.com/389625753): Investigate whether only passing the
  // field type is the right choice here. This would for example
  // fail the fill a PASSPORT_NUMBER field that gets a
  // PHONE_HOME_WHOLE_NUMBER classification from regular autofill
  // prediction logic.
  std::u16string value =
      attribute.GetInfo(type, app_locale, field.format_string());
  switch (field.Type().GetStorableType()) {
    case ADDRESS_HOME_STATE:
      // TODO(crbug.com/389625753): Support countries other than the US.
      return GetStateTextForInput(value, /*country_code=*/"US",
                                  field.max_length(),
                                  /*failure_to_fill=*/nullptr);
    default:
      return value;
  }
}

std::u16string GetValueForSelectControl(const AttributeInstance& attribute,
                                        const AutofillField& field,
                                        const std::string& app_locale,
                                        AddressNormalizer* address_normalizer) {
  FieldType type = field.Type().GetStorableType();
  if (IsDateFieldType(type)) {
    return GetValueForDateSelectControl(attribute, field, app_locale)
        .value_or(u"");
  }
  std::u16string fill_value = GetValueForInput(attribute, field, app_locale);
  if (fill_value.empty()) {
    return u"";
  }

  switch (type) {
    case ADDRESS_HOME_COUNTRY:
      return GetCountrySelectControlValue(fill_value, field.options(),
                                          /*failure_to_fill=*/nullptr);
    case ADDRESS_HOME_STATE:
      // TODO(crbug.com/389625753): Support countries other than the US.
      return GetStateSelectControlValue(fill_value, field.options(),
                                        /*country_code=*/"US",
                                        address_normalizer,
                                        /*failure_to_fill=*/nullptr);
    default:
      return GetSelectControlValue(fill_value, field.options(),
                                   /*failure_to_fill=*/nullptr)
          .value_or(u"");
  }
}

}  // namespace

base::flat_set<FieldGlobalId> GetFieldsFillableByAutofillAi(
    const FormStructure& form,
    const AutofillClient& client) {
  const EntityDataManager* const edm = client.GetEntityDataManager();
  if (!MayPerformAutofillAiAction(client, AutofillAiAction::kFilling) || !edm) {
    return {};
  }
  auto fillable_by_autofill_ai =
      [&, fillable_types =
              std::optional<FieldTypeSet>()](FieldType field_type) mutable {
        if (!fillable_types) {
          fillable_types.emplace();
          for (const EntityInstance& entity : edm->GetEntityInstances()) {
            for (const AttributeInstance& attribute : entity.attributes()) {
              fillable_types->insert(attribute.type().field_type());
            }
          }
        }
        return fillable_types->contains(field_type);
      };

  std::vector<FieldGlobalId> fields;
  for (const auto& field : form.fields()) {
    std::optional<FieldType> field_type =
        field->GetAutofillAiServerTypePredictions();
    if (field_type && fillable_by_autofill_ai(*field_type)) {
      fields.push_back(field->global_id());
    }
  }
  return std::move(fields);
}

std::u16string GetFillValueForEntity(
    const EntityInstance& entity,
    const AutofillField& field,
    mojom::ActionPersistence action_persistence,
    const std::string& app_locale,
    AddressNormalizer* address_normalizer) {
  std::optional<FieldType> field_type =
      field.GetAutofillAiServerTypePredictions();
  if (!field_type) {
    return u"";
  }
  std::optional<AttributeType> attribute_type =
      AttributeType::FromFieldType(*field_type);
  if (!attribute_type || attribute_type->entity_type() != entity.type()) {
    return u"";
  }

  base::optional_ref<const AttributeInstance> attribute =
      entity.attribute(*attribute_type);
  if (!attribute) {
    return u"";
  }

  std::u16string fill_value =
      field.IsSelectElement()
          ? GetValueForSelectControl(*attribute, field, app_locale,
                                     address_normalizer)
          : GetValueForInput(*attribute, field, app_locale);

  const bool should_obfuscate =
      action_persistence != mojom::ActionPersistence::kFill &&
      !field.IsSelectElement() && attribute->type().is_obfuscated();

  // TODO(crbug.com/394011769): Investigate whether the obfuscation should
  // should include some of the attribute's value, e.g. the last x characters.
  return should_obfuscate ? GetObfuscatedValue(fill_value) : fill_value;
}

}  // namespace autofill
