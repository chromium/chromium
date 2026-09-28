// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_TYPES_H_
#define COMPONENTS_ORIGIN_GATING_CORE_TYPES_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "base/check.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/origin_gating/core/concepts.h"
#include "url/gurl.h"

namespace origin_gating {

// Enumerate the result of a single predicate check.
enum class Decision {
  // The predicate neither explicitly allowed nor explicitly blocked the event.
  kNoDecision,
  // The predicate explicitly allowed the event.
  kAllowed,
  // The predicate explicitly blocked the event.
  kBlocked,
};

struct NavigationRequestEvent {
  friend bool operator==(const NavigationRequestEvent&,
                         const NavigationRequestEvent&) = default;

  GURL source;
  GURL destination;
};

struct NavigationResponseEvent {
  friend bool operator==(const NavigationResponseEvent&,
                         const NavigationResponseEvent&) = default;

  GURL source;
  GURL destination;
};

struct PageActionEvent {
  friend bool operator==(const PageActionEvent&,
                         const PageActionEvent&) = default;

  GURL destination;
};

// The event being evaluated by origin gating. A single predicate
// may apply to some events but not others (e.g. dangerous MIME type check is
// applicable only to navigation response), so callers pass the relevant event
// to ComputeGatingDecision and each predicate declares the set of events it
// applies to.
class GateableEvent {
 public:
  enum class Type {
    // A navigation request is starting, or is being redirected.
    kNavigationRequest,
    // A navigation response is being processed (the final, committed URL).
    kNavigationResponse,
    // An action is being performed on an existing tab/page.
    kPageAction,
  };

  using enum Type;

  explicit GateableEvent(NavigationRequestEvent event);
  explicit GateableEvent(NavigationResponseEvent event);
  explicit GateableEvent(PageActionEvent event);

  GateableEvent(const GateableEvent&);
  GateableEvent& operator=(const GateableEvent&);
  GateableEvent(GateableEvent&&);
  GateableEvent& operator=(GateableEvent&&);
  ~GateableEvent();

  friend bool operator==(const GateableEvent&, const GateableEvent&) = default;

  Type type() const;

  bool is_navigation_request() const {
    return type() == Type::kNavigationRequest;
  }
  bool is_navigation_response() const {
    return type() == Type::kNavigationResponse;
  }
  bool is_page_action() const { return type() == Type::kPageAction; }

  // Returns the target/destination URL common to all events.
  const GURL& destination() const;

  // Returns the navigation source URL for `NavigationRequestEvent` and
  // `NavigationResponseEvent`, or `nullptr` for `PageActionEvent`.
  const GURL* source() const;

  const NavigationRequestEvent* GetIfNavigationRequest() const {
    return std::get_if<NavigationRequestEvent>(&data_);
  }
  const NavigationResponseEvent* GetIfNavigationResponse() const {
    return std::get_if<NavigationResponseEvent>(&data_);
  }
  const PageActionEvent* GetIfPageAction() const {
    return std::get_if<PageActionEvent>(&data_);
  }

 private:
  // Order of alternatives must match `Type` enumerators.
  std::variant<NavigationRequestEvent, NavigationResponseEvent, PageActionEvent>
      data_;
};

std::string GateableEventTypeToString(GateableEvent::Type type);

using GateableEventSet = base::EnumSet<GateableEvent::Type,
                                       GateableEvent::kNavigationRequest,
                                       GateableEvent::kPageAction>;

// Enumerates the source of any positive/negative decision.
enum class DecisionSource {
  // Predicate that allows if the origins in question are same-origin with each
  // other. Not supported for `GateableEvent::kPageAction` events.
  kAllowSameOrigin,
  // Predicate that allows if the destination is a localhost URL with an http or
  // https scheme.
  kAllowHttpLocalhost,
  // Predicate that allows if the destination is about:blank.
  kAllowAboutBlank,
  // Predicate that allows if the user has already confirmed the origin in
  // question.
  kCacheWithUserConfirmation,
  // Predicate that allows if the origin is already present in the cache and the
  // delegate does not require user confirmation for that origin.
  kCacheWithoutUserConfirmation,
  // Evaluates the destination against an enterprise policy allow/blocklist. The
  // delegate provides the embedder-specific logic via
  // `Delegate::EvaluateEnterprisePolicy`.
  kEnterprisePolicy,
  // Predicate that blocks if the destination's host is an IP address, unless
  // it is a loopback/localhost IP address.
  kForbidNonLocalhostIpAddress,
  // Predicate that blocks if the destination's scheme is not https, unless the
  // destination is localhost.
  kRequireHttpsOrLocalhost,
  // Predicate that blocks if the destination's scheme is neither https nor
  // http.
  kRequireHttpsOrHttp,
  // Predicate that evaluates the destination against the task policy
  // configuration and blocks if blocked, otherwise returns kNoDecision.
  kBlockByTaskPolicyConfig,
  // Predicate that evaluates the destination against the task policy
  // configuration and allows if allowed, otherwise returns kNoDecision.
  kAllowByTaskPolicyConfig,
  // No decision was reached before the OriginGating framework ran out of
  // predicates to run.
  kNoVerdict,
};

std::string DecisionSourceToString(DecisionSource source);

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

// Struct wrapping the final gating verdict and its resolution metadata.
struct GatingDecision {
  bool is_allowed = false;
  DecisionAttribution attribution;
};

// Opaque virtual base class for consumer-specific context passed along the
// gating decision chain.
class GatingDecisionContext {
 public:
  virtual ~GatingDecisionContext() = default;
};

using GatingDecisionCallback =
    base::OnceCallback<void(std::unique_ptr<GatingDecisionContext>,
                            GatingDecision)>;

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_TYPES_H_
