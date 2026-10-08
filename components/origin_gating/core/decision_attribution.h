// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_DECISION_ATTRIBUTION_H_
#define COMPONENTS_ORIGIN_GATING_CORE_DECISION_ATTRIBUTION_H_

#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ref.h"
#include "components/origin_gating/core/concepts.h"
#include "components/origin_gating/core/decision_source.h"

namespace origin_gating {

// An opaque domain tag identifying an enum type used for custom predicates.
// Each enum type provides a singleton `kInstance<E>` value.
struct CustomPredicateDomain {
  template <IsIntCompatibleEnum E>
  static const CustomPredicateDomain kInstance;
};

// Encapsulates the source of any positive/negative gating verdict.
class DecisionAttribution {
 public:
  enum class Type {
    kDecisionSource,
    kCustomPredicate,
  };

  class CustomPredicateAttribution {
   public:
    template <IsIntCompatibleEnum E>
    explicit CustomPredicateAttribution(E id)
        : id_(static_cast<int>(id)),
          domain_(CustomPredicateDomain::kInstance<E>) {}

    friend bool operator==(const CustomPredicateAttribution&,
                           const CustomPredicateAttribution&) = default;

    // CHECKs that `domain_` matches `E`.
    template <IsIntCompatibleEnum E>
    E GetId() const {
      CHECK_EQ(&domain_.get(), &CustomPredicateDomain::kInstance<E>);
      return static_cast<E>(id_);
    }

    bool IsSameDomain(const CustomPredicateAttribution& other) const {
      return &domain_.get() == &other.domain_.get();
    }

   private:
    int id_ = 0;
    raw_ref<const CustomPredicateDomain> domain_;
  };

  DecisionAttribution() = delete;
  explicit DecisionAttribution(DecisionSource source);
  explicit DecisionAttribution(const CustomPredicateAttribution& attribution);

  ~DecisionAttribution();
  DecisionAttribution(const DecisionAttribution&);
  DecisionAttribution& operator=(const DecisionAttribution&);
  DecisionAttribution(DecisionAttribution&&);
  DecisionAttribution& operator=(DecisionAttribution&&);

  Type type() const;

  // Returns the DecisionSource. Safe to call only when `type()` is
  // `Type::kDecisionSource`.
  DecisionSource Source() const;

  // Returns the custom predicate ID. Safe to call only when `type()` is
  // `Type::kCustomPredicate`. `E` must be the same type that was used to create
  // the corresponding `CustomPredicate`.
  template <IsIntCompatibleEnum E>
  E CustomPredicateId() const {
    CHECK(is_custom_predicate());
    return std::get<CustomPredicateAttribution>(attribution_).GetId<E>();
  }

  bool operator==(DecisionSource source) const;

  // Compares against a given source enum. Returns false if the enum type
  // doesn't match the type used to create the corresponding `CustomPredicate`.
  template <IsIntCompatibleEnum E>
  bool operator==(E id) const {
    if (!is_custom_predicate()) {
      return false;
    }
    return std::get<CustomPredicateAttribution>(attribution_) ==
           CustomPredicateAttribution(id);
  }

 private:
  bool is_source() const { return type() == Type::kDecisionSource; }
  bool is_custom_predicate() const { return type() == Type::kCustomPredicate; }

  std::variant<DecisionSource, CustomPredicateAttribution> attribution_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_DECISION_ATTRIBUTION_H_
