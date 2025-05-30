// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_INSTANCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_INSTANCE_H_

#include <ostream>
#include <string>
#include <variant>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/optional_ref.h"
#include "base/types/strong_alias.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/contact_info.h"
#include "components/autofill/core/browser/data_model/autofill_ai/country_info.h"
#include "components/autofill/core/browser/data_model/autofill_ai/date_info.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/is_required.h"

namespace autofill {

// Entity and attribute types are blueprints for entity and attribute instances.
//
// For example, "passport" is an entity type and its attribute types include
// "name", "country", "issue date", "expiry date", etc.
//
// A specific passport is an entity instance, which has attribute instances with
// values such "John Doe", "USA", "05/2019", "04/2029", etc.
//
// Entity instances are loaded from a webdata table and exposed through
// EntityDataManager.
class AttributeInstance;
class EntityInstance;
class EntityTable;

// An attribute instance is a typed string value with additional metadata.
// It is associated with an EntityInstance. Attributes are used in order to fill
// fields with information of certain types.
//
// Note that there are two concepts of types that are relevant here:
// - AttributeType: This is the type of the attribute itself and determines the
//   structure of the attribute.
// - FieldType: This is the type of data that can be requested by consumers from
//   the attribute.
//
// `AutofillField` computes two types for the field: One is available through
// `AutofillField::GetAutofillAiServerTypePredictions()` and represents the
// type used in order to figure out the appropriate AttributeInstance to fill
// the field. The other is available through `AutofillField::Type()` and
// represents the general classification of the field (through Autofill server
// and heuristic prediction logic).
//
// It could happen that these two types are totally unrelated (e.g., the former
// returns PASSPORT_NAME_TAG and the latter returns PHONE_HOME_WHOLE_NUMBER)
// or that the two types are equal (e.g., both return PASSPORT_NAME_TAG). This
// is a small problem for setter/getter API that (1) assumes that the provided
// field type to a given method is supported and (2) doesn't have support for
// `FieldType`s of group `FieldTypeGroup::kAutofillAi`. See
// `AttributeInstance::GetNormalizedType()` and the getter/setter methods for
// how this problem is handled.
class AttributeInstance final {
  using StateInfo = base::StrongAlias<class StateInfoTag, std::u16string>;
  using InfoStructure =
      std::variant<CountryInfo, DateInfo, NameInfo, StateInfo, std::u16string>;

 public:
  // Transparent less-than relation based on the AttributeType.
  struct CompareByType;

  // Comparator that ranks types by their priority for disambiguating different
  // instances of the same entity type, as specified in the schema.
  // `DisambiguationOrder(x, y) == true` means `x` has higher priority than `y`.
  static bool DisambiguationOrder(const AttributeInstance& lhs,
                                  const AttributeInstance& rhs);

  explicit AttributeInstance(AttributeType type);

  AttributeInstance(const AttributeInstance&);
  AttributeInstance& operator=(const AttributeInstance&);
  AttributeInstance(AttributeInstance&&);
  AttributeInstance& operator=(AttributeInstance&&);
  ~AttributeInstance();

  const AttributeType& type() const { return type_; }

  // In the functions below, `type` refers to the type of data we want to fetch
  // from the attribute, and not the type of the attribute itself. The two might
  // coincide for unstructured types but they are different for structured
  // types. See `GetNormalizedType()` below for more information about the
  // correlation between the needed data type and the type of the attribute.
  // Also note that `type` below is mostly interesting for structured attributes
  // and is assumed to be just the attribute-type-equivalent field type for
  // unstructured ones.

  // Returns a string that contains all information stored in this attribute
  // instance, formatted according to the given `app_locale`.
  //
  // For more control over over which, see GetInfo().
  std::u16string GetCompleteInfo(const std::string& app_locale) const {
    return GetInfo(type().field_type(), app_locale, std::nullopt);
  }

  // Returns the value stored in this attribute instance for a specific `type`,
  // formatted according to a given `app_locale` and `format_string`.
  //
  // Currently, the `format_string` only matters for dates. If it is empty, it
  // defaults to u"YYYY-MM-DD". See AutofillField::format_string() for the
  // grammar of format strings.
  std::u16string GetInfo(
      FieldType type,
      const std::string& app_locale,
      base::optional_ref<const std::u16string> format_string) const;

  class GetRawInfoPassKey {
    constexpr GetRawInfoPassKey() = default;
    friend class AttributeInstance;
    friend class EntityInstance;
    friend class EntityTable;
  };

  // Same as `GetInfo` but returns the value as stored with no formatting
  // whatsoever.
  std::u16string GetRawInfo(GetRawInfoPassKey pass_key, FieldType type) const;

  // Returns the verification status of a value stored in this attribute
  // instance for a specific `type`.
  VerificationStatus GetVerificationStatus(FieldType type) const;

  // Populates the attribute with a value for a specific `type`, according to a
  // given `app_locale`.
  //
  // Currently, the `format_string` only matters for dates. Dates are updated
  // incrementally, e.g., SetInfo(..., u"16", ..., u"DD", ...) only changes the
  // day and does not reset the month or year. If `value` doesn't fully match
  // the `format_string`, the function is a no-op, e.g.,
  // SetInfo(..., u"16/12/2022", ..., u"DD", ...) is a no-op.
  // See AutofillField::format_string() for the grammar of format strings.
  void SetInfo(FieldType type,
               const std::u16string& value,
               const std::string& app_locale,
               std::u16string_view format_string,
               VerificationStatus status);

  // Same as `SetInfoWithVerificationStatus`, but for structured types this
  // function does nothing but modify the information in `type`, while the other
  // function might perform additional steps (e.g., name formatting). This
  // function should only be used by database logic and settings page logic.
  // TODO(crbug.com/389625753): Investigate merging SetInfo* and SetRawInfo*.
  void SetRawInfo(FieldType type,
                  const std::u16string& value,
                  VerificationStatus status);

  // Returns the set of `FieldType`s for which the setter/getter functions above
  // may be called.
  FieldTypeSet GetSupportedTypes() const;

  // Returns the types which are stored in the database for this attribute
  // to be able to correctly reconstruct it at database loading time.
  FieldTypeSet GetDatabaseStoredTypes() const;

  // This is a no-op for unstructured attributes, and for structured attributes
  // the function propagates changes in a component to its subcomponents. This
  // should be called when constructing a structured attribute object from
  // scratch (e.g., Loading the object from the database, creating an import
  // candidate).
  void FinalizeInfo();

  friend bool operator==(const AttributeInstance& lhs,
                         const AttributeInstance& rhs) = default;

 private:
  // This function checks that `info_type` is supported by the attribute and
  // otherwise tries to convert it into one that is. Returns the supported type
  // if found and UNKNOWN_TYPE otherwise.
  FieldType GetNormalizedType(FieldType info_type) const;

  AttributeType type_;
  InfoStructure info_;
};

struct AttributeInstance::CompareByType {
  using is_transparent = void;

  bool operator()(const AttributeInstance& lhs, AttributeType rhs) const {
    return lt(lhs.type().name(), rhs.name());
  }

  bool operator()(AttributeType lhs, const AttributeInstance& rhs) const {
    return lt(lhs.name(), rhs.type().name());
  }

  bool operator()(const AttributeInstance& lhs,
                  const AttributeInstance& rhs) const {
    return lt(lhs.type().name(), rhs.type().name());
  }

 private:
  bool lt(AttributeTypeName lhs, AttributeTypeName rhs) const {
    return base::to_underlying(lhs) < base::to_underlying(rhs);
  }
};

// An entity instance is a non-empty set of attribute instances with additional
// metadata. The type is an EntityType.
class EntityInstance final {
 public:
  // `attributes` must be non-empty and their type must be identical to `type`.
  EntityInstance(EntityType type,
                 base::flat_set<AttributeInstance,
                                AttributeInstance::CompareByType> attributes,
                 base::Uuid guid,
                 std::string nickname,
                 base::Time date_modified,
                 size_t use_count,
                 base::Time use_date);

  EntityInstance(const EntityInstance&);
  EntityInstance& operator=(const EntityInstance&);
  EntityInstance(EntityInstance&&);
  EntityInstance& operator=(EntityInstance&&);
  ~EntityInstance();

  // Transparent less-than relation based on the the GUID.
  struct CompareByGuid;

  // Comparator that returns the entity with the higher frecency score.
  struct RankingOrder {
   public:
    explicit RankingOrder(base::Time now);
    bool operator()(const EntityInstance& lhs, const EntityInstance& rhs) const;

   private:
    const base::Time now_;
  };

  // Comparator that ranks instances by their priority for import on form
  // submission.
  // `ImportOrder(x, y) == true` means `x` has higher priority than `y`.
  static bool ImportOrder(const EntityInstance& lhs, const EntityInstance& rhs);

  const EntityType& type() const { return type_; }

  // The attributes present in this instance.
  // This is a subset of the attributes supported by the entity type.
  base::span<const AttributeInstance> attributes() const LIFETIME_BOUND {
    return attributes_;
  }

  // Returns the instance of `a` if it is present.
  base::optional_ref<const AttributeInstance> attribute(AttributeType a) const
      LIFETIME_BOUND {
    CHECK_EQ(a.entity_type(), type());
    auto it = attributes_.find(a);
    return it != attributes_.end() ? &*it : nullptr;
  }

  // Globally unique identifier of this entity.
  const base::Uuid& guid() const LIFETIME_BOUND { return guid_; }

  // The nickname assigned to this instance by the user.
  const std::string& nickname() const LIFETIME_BOUND { return nickname_; }

  // The latest time the instance, including any of its attributes, was edited.
  base::Time date_modified() const { return date_modified_; }

  // Updates the last time an entity was used to fill a form and
  // increases the entity use count.
  void RecordEntityUsed(base::Time date);

  // Returns the last time an entity was used to fill a form.
  base::Time use_date() const { return use_date_; }

  // Returns how many times an entity was used to fill a form.
  size_t use_count() const { return use_count_; }

  struct EntityMergeability {
    EntityMergeability();
    EntityMergeability(std::vector<AttributeInstance> mergeable_attributes,
                       bool is_subset);
    EntityMergeability(const EntityMergeability&);
    EntityMergeability(EntityMergeability&&);
    EntityMergeability& operator=(const EntityMergeability&);
    EntityMergeability& operator=(EntityMergeability&&);
    ~EntityMergeability();

    // Given two instances (caller and parameter), this specifies the values of
    // the second instance which can be merged in the first one (caller). This
    // is not present if `is_subset` is true.
    std::vector<AttributeInstance> mergeable_attributes;

    bool is_subset = false;
  };

  // - If `this` is a proper superset of `newer`,
  //   `EntityMergeability::mergeable_attributes` contains the list of
  //   attributes that `newer` has, but `this` does not. These attributes can be
  //   set on `this` to update it.
  // - If `newer` is a proper subset of `this`,
  //   `EntityMergeability::mergeable_attributes` is empty and
  //   `EntityMergeability::is_subset` is `true`. In this case no saving or
  //   updating is required.
  // - If `newer` and `this` have a matching merge constraints, the values of
  //   `newer` should be merged into `this` and
  //   `EntityMergeability::mergeable_attributes` will not be empty.
  // - Otherwise, `newer` should be considered an independent entity.
  // TODO(389629676): This does not yet properly handle names and possibly
  // dates.
  EntityMergeability GetEntityMergeability(const EntityInstance& newer) const;

  friend bool operator==(const EntityInstance&,
                         const EntityInstance&) = default;

 private:
  EntityType type_;
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes_;
  base::Uuid guid_;
  std::string nickname_;
  base::Time date_modified_;
  size_t use_count_;
  base::Time use_date_;
};

std::ostream& operator<<(std::ostream& os, const AttributeInstance& a);
std::ostream& operator<<(std::ostream& os, const EntityInstance& e);

struct EntityInstance::CompareByGuid {
  using is_transparent = void;

  bool operator()(const EntityInstance& lhs, const base::Uuid& rhs) const {
    return lhs.guid() < rhs;
  }

  bool operator()(const base::Uuid& lhs, const EntityInstance& rhs) const {
    return lhs < rhs.guid();
  }

  bool operator()(const EntityInstance& lhs, const EntityInstance& rhs) const {
    return lhs.guid() < rhs.guid();
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_INSTANCE_H_
