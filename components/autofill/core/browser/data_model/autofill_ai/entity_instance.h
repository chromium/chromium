// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_INSTANCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_INSTANCE_H_

#include <ostream>
#include <string>
#include <string_view>
#include <variant>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
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
struct AutofillFormatString;
class EntityInstance;
class EntityInstanceTestApi;
class EntityTable;

// An attribute instance is a typed string value with additional metadata.
// It is associated with an EntityInstance. Attributes are used in order to fill
// fields with information of certain types.
class AttributeInstance final {
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

  // Returns a string that contains all information stored in this attribute
  // instance, formatted according to the given `app_locale`.
  //
  // For more control over the return value, see GetInfo().
  std::u16string GetCompleteInfo(std::string_view app_locale) const {
    return GetInfo(type_.field_type(), app_locale, std::nullopt);
  }

  // Returns a string that contains the raw information stored in this attribute
  // instance.
  //
  // For more control over the return value, see GetRawInfo().
  std::u16string GetCompleteRawInfo() const {
    return GetRawInfo(type_.field_type());
  }

  // Returns the value stored in this attribute instance.
  //
  // The `field_type` may be any of `type().field_subtypes()`; otherwise we fall
  // back to `type().field_type()`. That is, the `field_type` only matters for
  // name attributes.
  //
  // Currently, the `format_string` only matters for dates. If it is empty, it
  // defaults to u"YYYY-MM-DD". See AutofillField::format_string() for the
  // grammar of format strings.
  std::u16string GetInfo(
      FieldType field_type,
      std::string_view app_locale,
      base::optional_ref<const AutofillFormatString> format_string) const;

  // Same as `GetInfo` but returns the value as stored with no formatting
  // whatsoever.
  //
  // See GetInfo() for the meaning of `field_type`.
  std::u16string GetRawInfo(FieldType field_type) const;

  // Returns the verification status of a value stored in this attribute
  // instance for a specific `type`.
  //
  // See GetInfo() for the meaning of `field_type`.
  VerificationStatus GetVerificationStatus(FieldType field_type) const;

  // Populates the attribute with a value for a specific `type`, according to a
  // given `app_locale`.
  //
  // See GetInfo() for the meaning of `field_type`.
  //
  // Currently, the `format_string` only matters for dates. Dates are updated
  // incrementally, e.g.,
  //   SetInfo(..., u"16", ...,
  //           AutofillFormatString::FromDateFormat(u"DD"), ...);
  // only changes the day and does not reset the month or year. If `value`
  // doesn't fully match the `format_string`, e.g.
  //   SetInfo(..., u"16/12/2022", ...,
  //           AutofillFormatString::FromDateFormat(u"DD"), ...);
  // the function is a no-op.
  // See AutofillField::format_string() for the grammar of format strings.
  void SetInfo(FieldType field_type,
               const std::u16string& value,
               std::string_view app_locale,
               base::optional_ref<const AutofillFormatString> format_string,
               VerificationStatus status);

  // Similar to SetInfo() but without canonicalization: It does not accept
  // country names and does not format names. This function should only be used
  // by database logic and settings page logic.
  // TODO(crbug.com/389625753): Investigate merging SetInfo* and SetRawInfo*.
  void SetRawInfo(FieldType field_type,
                  const std::u16string& value,
                  VerificationStatus status);

  // This is a no-op for unstructured attributes, and for structured attributes
  // the function propagates changes in a component to its subcomponents. This
  // should be called when constructing a structured attribute object from
  // scratch (e.g., Loading the object from the database, creating an import
  // candidate).
  void FinalizeInfo();

  friend bool operator==(const AttributeInstance& lhs,
                         const AttributeInstance& rhs) = default;

 private:
  using StateInfo = base::StrongAlias<class StateInfoTag, std::u16string>;
  using InfoStructure =
      std::variant<CountryInfo, DateInfo, NameInfo, StateInfo, std::u16string>;

  FieldType GetNormalizedFieldType(FieldType field_type) const;

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
  // A globally unique identifier for entities.
  // Use `base::Uuid` whenever you can for new entities, as it would be
  // preferred to migrate from this to `base::Uuid`, which is currently not
  // possible unfortunately because some legacy entities still have IDs with
  // different formats.
  struct EntityId : public base::StrongAlias<struct EntityIdTag, std::string> {
   public:
    using base::StrongAlias<struct EntityIdTag, std::string>::StrongAlias;
    explicit EntityId(const base::Uuid uuid)
        : EntityId(uuid.AsLowercaseString()) {}
  };

  // Contains information about an entity's metadata stored in the
  // `entities_metadata` table.
  struct EntityMetadata {
    EntityInstance::EntityId guid;
    base::Time date_modified;
    size_t use_count;
    base::Time use_date;

    friend bool operator==(const EntityMetadata&,
                           const EntityMetadata&) = default;
  };

  // Controls whether the attributes of the entity instance can be edited by the
  // user.
  using AreAttributesReadOnly =
      base::StrongAlias<class AreAttributesReadOnlyTag, bool>;

  // These values are persisted to a database. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class RecordType {
    // The entity was created/saved locally, it exists only in the local
    // `EntityTable`.
    kLocal = 0,
    // The entity is stored in Wallet and the current instance is only a local
    // copy. Changes happening locally or on the Wallet server are synced among
    // all local storages sharing this entity.
    kServerWallet = 1,
    kMaxValue = kServerWallet,
  };

  // `attributes` must be non-empty and their type must be identical to `type`.
  EntityInstance(EntityType type,
                 base::flat_set<AttributeInstance,
                                AttributeInstance::CompareByType> attributes,
                 EntityId guid,
                 std::string nickname,
                 base::Time date_modified,
                 size_t use_count,
                 base::Time use_date,
                 RecordType record_type,
                 AreAttributesReadOnly are_attributes_read_only,
                 std::string frecency_override);

  EntityInstance(const EntityInstance&);
  EntityInstance& operator=(const EntityInstance&);
  EntityInstance(EntityInstance&&);
  EntityInstance& operator=(EntityInstance&&);
  ~EntityInstance();

  // Transparent less-than relation based on the the GUID.
  struct CompareByGuid;

  // Comparator that returns the entity with the higher frecency score.
  // If both entities have non-empty frecency override, the one with the lowest
  // lexicographical order of the override string will be first.
  // If one entity has a non-empty frecency override and the other does not,
  // the entity with the override will be first.
  struct FrecencyOrder {
   public:
    explicit FrecencyOrder(base::Time now);
    bool operator()(const EntityInstance& lhs, const EntityInstance& rhs) const;

   private:
    const base::Time now_;
  };

  // Comparator that ranks instances by their priority for import on form
  // submission.
  // `ImportOrder(x, y) == true` means `x` has higher priority than `y`.
  static bool ImportOrder(const EntityInstance& lhs, const EntityInstance& rhs);

  // Comparator that ranks instances by their priority for server migration on
  // form submission. `MigrationOrder(x, y) == true` means `x` has higher
  // priority than `y`.
  static bool MigrationOrder(const EntityInstance& lhs,
                             const EntityInstance& rhs);

  const EntityType& type() const { return type_; }

  // The attributes present in this instance.
  // This is a subset of the attributes supported by the entity type.
  base::span<const AttributeInstance> attributes() const LIFETIME_BOUND {
    return attributes_;
  }

  // Returns the instance of `a` if it is present.
  base::optional_ref<const AttributeInstance> attribute(AttributeType a) const
      LIFETIME_BOUND {
    CHECK_EQ(a.entity_type(), type_);
    auto it = attributes_.find(a);
    return it != attributes_.end() ? &*it : nullptr;
  }

  // Globally unique identifier of this entity.
  const EntityId& guid() const LIFETIME_BOUND { return guid_; }

  // The nickname assigned to this instance by the user.
  const std::string& nickname() const LIFETIME_BOUND { return nickname_; }

  // The latest time the instance, including any of its attributes, was edited.
  base::Time date_modified() const { return entity_metadata_.date_modified; }

  // Updates the last time an entity was used to fill a form and
  // increases the entity use count.
  void RecordEntityUsed(base::Time date);

  // Returns the last time an entity was used to fill a form.
  base::Time use_date() const { return entity_metadata_.use_date; }

  // Returns how many times an entity was used to fill a form.
  size_t use_count() const { return entity_metadata_.use_count; }

  // Returns the metadata for this instance.
  const EntityMetadata& metadata() const { return entity_metadata_; }

  // Sets the metadata for this instance.
  void set_metadata(EntityMetadata metadata) {
    CHECK_EQ(guid_, metadata.guid);
    entity_metadata_ = std::move(metadata);
  }

  // Returns true if the attributes of this entity instance cannot be edited by
  // the user.
  AreAttributesReadOnly are_attributes_read_only() const {
    return are_attributes_read_only_;
  }

  // Returns the type of storage used for the specific entity.
  RecordType record_type() const { return record_type_; }

  // Returns the ordering override for the specific entity.
  const std::string& frecency_override(
      base::PassKey<EntityTable> pass_key) const {
    return frecency_override_;
  }

  // Whether the instance's `record_type` indicates server side storage.
  bool IsServerInstance() const;

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

  // - If `newer` is a proper superset of `this`,
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

  // Returns true if all attributes of `this` are present in `other` with the
  // same values or if `this` is a proper subset of `other`.
  bool IsSubsetOf(const EntityInstance& other) const;

  friend bool operator==(const EntityInstance&,
                         const EntityInstance&) = default;

 private:
  friend class EntityInstanceTestApi;

  EntityType type_;
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes_;
  EntityId guid_;
  std::string nickname_;
  EntityMetadata entity_metadata_;
  RecordType record_type_;
  AreAttributesReadOnly are_attributes_read_only_;
  std::string frecency_override_;
};

std::ostream& operator<<(std::ostream& os, const AttributeInstance& a);
std::ostream& operator<<(std::ostream& os, const EntityInstance& e);

struct EntityInstance::CompareByGuid {
  using is_transparent = void;

  bool operator()(const EntityInstance& lhs, const EntityId& rhs) const {
    return lhs.guid() < rhs;
  }

  bool operator()(const EntityId& lhs, const EntityInstance& rhs) const {
    return lhs < rhs.guid();
  }

  bool operator()(const EntityInstance& lhs, const EntityInstance& rhs) const {
    return lhs.guid() < rhs.guid();
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_INSTANCE_H_
