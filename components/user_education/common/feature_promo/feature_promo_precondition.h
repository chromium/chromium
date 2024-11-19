// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_PRECONDITION_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_PRECONDITION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/precondition_data.h"
#include "ui/base/interaction/element_identifier.h"

namespace user_education {

namespace internal {
class FeaturePromoQueueCachedDataTest;
class FeaturePromoQueueSetCachedDataTest;
}  // namespace internal

// Represents a single precondition for promos. Derive specific preconditions
// from this class.
class FeaturePromoPrecondition {
 public:
  using Identifier = ui::ElementIdentifier;

  // Boilerplate; this class is not copyable.
  FeaturePromoPrecondition(const FeaturePromoPrecondition&) = delete;
  void operator=(const FeaturePromoPrecondition&) = delete;
  virtual ~FeaturePromoPrecondition() = default;

  // Returns a unique identifier for different types of preconditions.
  virtual Identifier GetIdentifier() const = 0;

  // Gets a description of the precondition.
  virtual const std::string& GetDescription() const = 0;

  // Gets whether the precondition is met and promos are allowed. If not,
  // returns the relevant failure.
  virtual FeaturePromoResult CheckPrecondition() const = 0;

  // Extracts any cached data from this precondition and adds it to `to_add_to`;
  // future calls to this object may fail. Cached data likely reflects the most
  // recent time `IsAllowed()` was called, and therefore that method should
  // always be called first.
  virtual void ExtractCachedData(
      internal::PreconditionData::Collection& to_add_to) {}

 protected:
  FeaturePromoPrecondition() = default;
};

// Same as `FeaturePromoPrecondition`, but stores values for identifier
// and description, along with optional cached data.
class FeaturePromoPreconditionBase : public FeaturePromoPrecondition {
 public:
  using Identifier = ui::ElementIdentifier;

  // Boilerplate; this class is not copyable.
  ~FeaturePromoPreconditionBase() override;

  // FeaturePromoPrecondition:
  Identifier GetIdentifier() const override;
  const std::string& GetDescription() const override;
  void ExtractCachedData(
      internal::PreconditionData::Collection& to_add_to) override;

 protected:
  FeaturePromoPreconditionBase(Identifier identifier,
                               std::string description);

  // Use this method to initialize the various types of data the precondition
  // will support by passing in appropriate typed identifiers.
  //
  // Can be called any number of times with unique typed identifiers, or all
  // at once.
  template <typename... Args>
  void InitCache(internal::PreconditionData::TypedIdentifier<Args>... args) {
    (data_.emplace(
         args.identifier(),
         std::make_unique<internal::TypedPreconditionData<Args>>(args)),
     ...);
  }

  // Retrieve a reference to cached data held by the precondition, which can be
  // used to get or set the value. InitCache() must have been called with
  // the same `id`, and `ExtractData()` must not have been called.
  //
  // The data returned is mutable even thought the method is const, because it
  // is expected to be used to cache data.
  template <typename T>
  T& GetCachedData(internal::PreconditionData::TypedIdentifier<T> id) const {
    auto* const result = internal::PreconditionData::Get<T>(data_, id);
    CHECK(result);
    return *result;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(FeaturePromoPreconditionTest, SetAndGetCachedData);
  FRIEND_TEST_ALL_PREFIXES(FeaturePromoPreconditionTest,
                           GetCachedDataCrashesIfDataNotPresent);
  FRIEND_TEST_ALL_PREFIXES(FeaturePromoPreconditionTest, ExtractCachedData);
  FRIEND_TEST_ALL_PREFIXES(FeaturePromoPreconditionTest,
                           GetAfterExtractCachedDataFails);
  FRIEND_TEST_ALL_PREFIXES(FeaturePromoPreconditionTest,
                           FeaturePromoPreconditionList_ExtractCachedData);
  friend internal::FeaturePromoQueueCachedDataTest;
  friend internal::FeaturePromoQueueSetCachedDataTest;

  const Identifier identifier_;
  const std::string description_;

  // Mutable so that data can be cached during retrieval.
  mutable internal::PreconditionData::Collection data_;
};

// Represents a precondition that returns a cached value that is updated as it
// changes in realtime.
class CachingFeaturePromoPrecondition : public FeaturePromoPreconditionBase {
 public:
  CachingFeaturePromoPrecondition(Identifier identifier,
                                  std::string description,
                                  FeaturePromoResult initial_state);
  ~CachingFeaturePromoPrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition() const override;

  // See `set_is_allowed`.
  void set_check_result_for_testing(FeaturePromoResult check_result) {
    set_check_result(check_result);
  }

 protected:
  // Called by implementing classes to update the allowed state.
  void set_check_result(FeaturePromoResult check_result) {
    check_result_ = check_result;
  }

 private:
  FeaturePromoResult check_result_;
};

// Represents a precondition that forwards its allowed state from some other
// source of truth via a callback.
class CallbackFeaturePromoPrecondition : public FeaturePromoPreconditionBase {
 public:
  CallbackFeaturePromoPrecondition(
      Identifier identifier,
      std::string description,
      base::RepeatingCallback<FeaturePromoResult()> check_result_callback);
  ~CallbackFeaturePromoPrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition() const override;

 private:
  const base::RepeatingCallback<FeaturePromoResult()> check_result_callback_;
};

// Represents a precondition that forwards all of its information from another
// (longer-lived) source precondition.
class ForwardingFeaturePromoPrecondition : public FeaturePromoPrecondition {
 public:
  explicit ForwardingFeaturePromoPrecondition(
      const FeaturePromoPrecondition& source);
  ~ForwardingFeaturePromoPrecondition() override;

  // FeaturePromoPrecondition:
  Identifier GetIdentifier() const override;
  const std::string& GetDescription() const override;
  FeaturePromoResult CheckPrecondition() const override;

 private:
  raw_ref<const FeaturePromoPrecondition> source_;
};

// Represents an ordered list of preconditions which will be checked (see
// `CheckPreconditions()`). Owns the precondition objects it contains.
//
// Preconditions are created per-list; if state needs to be maintained between
// creation of lists, a forwarding- or callback-based implementation can be
// used.
class FeaturePromoPreconditionList {
 public:
  using ListType = std::vector<std::unique_ptr<FeaturePromoPrecondition>>;

  // Represents the result of checking the precondition list.
  class CheckResult {
   public:
    CheckResult() = default;
    CheckResult(FeaturePromoResult result,
                FeaturePromoPrecondition::Identifier failed_precondition)
        : result_(result), failed_precondition_(failed_precondition) {}
    CheckResult(const CheckResult&) = default;
    CheckResult& operator=(const CheckResult&) = default;
    ~CheckResult() = default;

    FeaturePromoResult result() const { return result_; }
    std::optional<FeaturePromoResult::Failure> failure() const {
      return result_.failure();
    }
    FeaturePromoPrecondition::Identifier failed_precondition() const {
      return failed_precondition_;
    }
    explicit operator bool() const { return result_; }
    bool operator!() const { return !result_; }
    bool operator==(const CheckResult&) const = default;
    bool operator!=(const CheckResult&) const = default;

   private:
    // The result of checking the list; success if no preconditions failed.
    FeaturePromoResult result_;
    // The identifier of the precondition that failed, or a null if none did.
    FeaturePromoPrecondition::Identifier failed_precondition_;
  };

  template <typename... Args>
  explicit FeaturePromoPreconditionList(Args... preconditions) {
    (AddPrecondition(std::move(preconditions)), ...);
  }

  FeaturePromoPreconditionList(FeaturePromoPreconditionList&&) noexcept;
  FeaturePromoPreconditionList& operator=(
      FeaturePromoPreconditionList&&) noexcept;
  ~FeaturePromoPreconditionList();

  // Adds `precondition` to this list.
  void AddPrecondition(std::unique_ptr<FeaturePromoPrecondition> precondition);

  // Appends all of the preconditions from `other` to this list.
  void AppendAll(FeaturePromoPreconditionList other);

  // Checks that all preconditions in the list are met, in order, and returns
  // either the `failure()` and `identifier()` of the first that does not pass,
  // or `FeaturePromoResult::Success()` if all preconditions pass.
  CheckResult CheckPreconditions() const;

  // Extracts cached data from all preconditions into `to_add_to`.
  void ExtractCachedData(internal::PreconditionData::Collection& to_add_to);

 private:
  ListType preconditions_;
};

}  // namespace user_education

// These macros are used to declare `FeaturePromoPrecondition::Identifier`s.
// Use these instead of the element identifier ones in case the implementation
// of the precondition IDs changes.
#define DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(IdentifierName) \
  DECLARE_ELEMENT_IDENTIFIER_VALUE(IdentifierName)
#define DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(IdentifierName) \
  DEFINE_ELEMENT_IDENTIFIER_VALUE(IdentifierName)
#define DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE( \
    IdentifierName)                                               \
  DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(__FILE__, __LINE__, IdentifierName)

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_PRECONDITION_H_
