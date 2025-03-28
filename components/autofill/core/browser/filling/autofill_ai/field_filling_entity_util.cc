// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"

#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/memory/stack_allocated.h"
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
  template <typename T>
  base::span<T> subspan(base::span<T> s) const {
    return s.subspan(offset, length);
  }

  size_t offset = 0;
  size_t length = 0;
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

// Represents a sequence of SelectOptions that represent years, months, or
// days.
struct DatePartRange {
  STACK_ALLOCATED();

 public:
  // Returns the SelectOption that represents a normalized `value`.
  base::optional_ref<const SelectOption> get_by_value(uint32_t value) const {
    if (value < value_offset || value >= value_offset + options.size()) {
      return std::nullopt;
    }
    return options[value - value_offset];
  }

  // A subspan of a field's options that has represents a sequence of years,
  // months, or days..
  base::span<const SelectOption> options;

  // A numerical representation of `options.front()`.
  // For day ranges, it is 1.
  // For month ranges, it is 1.
  // For year ranges, it is in YYYY format. That is, if `options.front()` is
  // `SelectOption{.text = u"2025"}` or `SelectOption{.text = u"25"}`, then
  // in both cases it is 2025.
  uint32_t value_offset = std::numeric_limits<uint32_t>::max();
};

// Returns a subspan of `options` that represents days.
// The span is either empty or has 28 to 31 elements and its `first_value` is 1.
DatePartRange GetDayRange(base::span<const SelectOption> options) {
  // Months have 28 to 31 days. We tolerate two additional options (e.g.,
  // "Pick day" and "Unknown").
  if (options.size() < 28 || options.size() > 33) {
    return {};
  }

  {
    // The user-visible text must start at "1".
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::text);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (28 <= seq.length && seq.length <= 31 && nums[seq.offset] == 1) {
      return {seq.subspan(options), 1};
    }
  }
  {
    // The user-invisible values must start at "0" or "1".
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::value);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (28 <= seq.length && seq.length <= 31 && nums[seq.offset] <= 1) {
      return {seq.subspan(options), 1};
    }
  }
  return {};
}

// Returns a subspan of `options` that represents months.
// The span is either empty or 12 elements and its `first_value` is 1.
DatePartRange GetMonthRange(base::span<const SelectOption> options) {
  // There are 12 months. We tolerate two additional options (e.g.,
  // "Pick month" and "Unknown").
  if (options.size() < 12 || options.size() > 14) {
    return {};
  }

  bool saw_digits = false;
  {
    // The user-visible text must contain "1" to "12".
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::text);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (seq.length == 12 && nums[seq.offset] == 1) {
      return {seq.subspan(options), 1};
    }
    saw_digits |= seq.length > 0;
  }
  {
    // The user-invisible values must be "1" to "12" or "0" to "11".
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::value);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (seq.length == 12 && nums[seq.offset] <= 1) {
      return {seq.subspan(options), 1};
    }
    saw_digits |= seq.length > 0;
  }

  // If there are no numbers, perhaps the months are spelled out.
  if (!saw_digits && options.size() == 12) {
    return {options, 1};
  }
  return {};
}

// Returns a subspan of `options` that represents years.
// The span is either empty or has at least 3 elements and its `first_value` is
// a four-digit representation of a year.
DatePartRange GetYearRange(base::span<const SelectOption> options) {
  auto year_offset = [](base::span<const int> nums, Sequence seq) -> uint32_t {
    auto is_yyyy = [](int year) { return 1800 <= year && year <= 2200; };
    auto is_yy = [](int year) { return 0 <= year && year <= 99; };
    if (std::ranges::all_of(seq.subspan(nums), is_yyyy) && seq.length > 3) {
      return nums[seq.offset];
    }
    if (std::ranges::all_of(seq.subspan(nums), is_yy) && seq.length > 3 &&
        seq.length != 12 && (seq.length < 28 || seq.length > 31)) {
      return 2000 + nums[seq.offset];
    }
    return 0;
  };

  // We want Get{Year,Month,Day}Range() to be mutually exclusive. Despite the
  // checks for YY years in year_offset(), such ambiguities can happen if, for
  // example, the values are [1,...,12] and the years are [2001, ..., 2012] or
  // [00, ..., 24].
  auto is_month_or_day = [&options] {
    return !GetDayRange(options).options.empty() ||
           !GetMonthRange(options).options.empty();
  };

  {
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::text);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    uint32_t min = year_offset(nums, seq);
    if (min != 0 && !is_month_or_day()) {
      return {seq.subspan(options), min};
    }
  }
  {
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::value);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    uint32_t min = year_offset(nums, seq);
    if (min != 0 && !is_month_or_day()) {
      return {seq.subspan(options), min};
    }
  }
  return {};
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

  auto get_part = [&](std::u16string format_string, uint32_t min = 0,
                      uint32_t max =
                          std::numeric_limits<uint32_t>::max()) -> uint32_t {
    std::u16string s = attribute.GetInfo(field.Type().GetStorableType(),
                                         app_locale, format_string);
    unsigned int i = -1;
    return base::StringToUint(s, &i) && min <= i && i <= max
               ? i
               : std::numeric_limits<uint32_t>::max();
  };

  if (base::optional_ref<const SelectOption> match =
          GetDayRange(field.options()).get_by_value(get_part(u"D", 1, 31))) {
    return match->value;
  }
  if (base::optional_ref<const SelectOption> match =
          GetMonthRange(field.options()).get_by_value(get_part(u"M", 1, 12))) {
    return match->value;
  }
  if (base::optional_ref<const SelectOption> match =
          GetYearRange(field.options()).get_by_value(get_part(u"YYYY"))) {
    return match->value;
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
