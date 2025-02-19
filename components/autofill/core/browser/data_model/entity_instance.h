// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ENTITY_INSTANCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ENTITY_INSTANCE_H_

#include <ostream>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/is_required.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

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
class EntityInstance;
class AttributeInstance;

// An attribute instance is a typed string value with additional metadata.
// It is associated with an EntityInstance. The type is an AttributeType.
class AttributeInstance final : public FormGroup {
 public:
  // Metadata from the saving moment.
  // This is more or less a placeholder for now.
  // TODO(crbug.com/388590912): Figure out the details or delete Context.
  struct Context {
    Context();
    Context(const Context&);
    Context& operator=(const Context&);
    Context(Context&&);
    Context& operator=(Context&&);
    ~Context();

    friend bool operator==(const Context&, const Context&) = default;

    // Human-readable description of the format, e.g., "date in MM/YYYY".
    std::string format;
  };

  // Less-than relation based on the AttributeType.
  struct CompareByType;

  // Comparator that ranks types by their priority for disambiguating different
  // instances of the same entity type, as specified in the schema.
  // `DisambiguationOrder(x, y) == true` means `x` has higher priority than `y`.
  static bool DisambiguationOrder(const AttributeInstance& lhs,
                                  const AttributeInstance& rhs);

  AttributeInstance(AttributeType type, std::u16string value, Context context);

  AttributeInstance(const AttributeInstance&);
  AttributeInstance& operator=(const AttributeInstance&);
  AttributeInstance(AttributeInstance&&);
  AttributeInstance& operator=(AttributeInstance&&);
  ~AttributeInstance() override;

  const AttributeType& type() const { return type_; }

  // Typically a user-entered string, e.g., a date.
  const std::u16string& value() const { return value_; }

  // Returns the normalized version of `this` attribute instance value. This
  // normalization removes extra spaces, converts the value to lowercase and
  // removes some special characters. Its underlying implementation is
  // `AutofillProfileComparator::NormalizeForComparison()`.
  std::u16string NormalizedValue() const;

  // Metadata from the saving moment of the value.
  const Context& context() const { return context_; }

  // autofill::FormGroup:
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  std::u16string GetInfo(const AutofillType& type,
                         const std::string& app_locale) const override;
  VerificationStatus GetVerificationStatus(FieldType type) const override;
  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     const std::u16string& value,
                                     const std::string& app_locale,
                                     const VerificationStatus status) override;
  FieldTypeSet GetSupportedTypes() const override;

  friend bool operator==(const AttributeInstance& lhs,
                         const AttributeInstance& rhs);

 private:
  AttributeType type_;
  std::u16string value_;
  Context context_;
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
                 base::Time date_modified);

  EntityInstance(const EntityInstance&);
  EntityInstance& operator=(const EntityInstance&);
  EntityInstance(EntityInstance&&);
  EntityInstance& operator=(EntityInstance&&);
  ~EntityInstance();

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
};

std::ostream& operator<<(std::ostream& os, const AttributeInstance& a);
std::ostream& operator<<(std::ostream& os, const EntityInstance& e);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ENTITY_INSTANCE_H_
