// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_

#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "components/origin_gating/core/types.h"
#include "url/origin.h"

namespace origin_gating {

// Represents a custom, name-tagged check provided by the embedder.
class CustomPredicate {
 public:
  using AsyncPredicate = base::RepeatingCallback<void(
      const GatingDecisionContext* context,
      const GURL& source,
      const GURL& destination,
      base::OnceCallback<void(Decision)> callback)>;

  using SyncPredicate =
      base::RepeatingCallback<Decision(const GatingDecisionContext* context,
                                       const GURL& source,
                                       const GURL& destination)>;

  CustomPredicate(AsyncPredicate predicate, std::string_view name);
  CustomPredicate(SyncPredicate predicate, std::string_view name);
  ~CustomPredicate();

  CustomPredicate(const CustomPredicate&);
  CustomPredicate& operator=(const CustomPredicate&);

  void Run(const GatingDecisionContext* context,
           const GURL& source,
           const GURL& destination,
           base::OnceCallback<void(Decision)> callback) const;

  const std::string& name() const { return name_; }

 private:
  AsyncPredicate predicate_;
  std::string name_;
};

class OriginGatingConfiguration {
 public:
  using Predicate = std::variant<DecisionSource, CustomPredicate>;

  // `predicates` specifies the ordered sequence of decision predicates to
  // execute.
  //
  // The following internal/fallback states are strictly forbidden:
  // - `DecisionSource::kNoVerdict`
  OriginGatingConfiguration(std::initializer_list<Predicate> predicates,
                            bool use_site_keyed_cache);
  ~OriginGatingConfiguration();

  OriginGatingConfiguration(const OriginGatingConfiguration&);
  OriginGatingConfiguration& operator=(const OriginGatingConfiguration&);

  const std::vector<Predicate>& predicates() const { return predicates_; }
  bool use_site_keyed_cache() const { return use_site_keyed_cache_; }

 private:
  std::vector<Predicate> predicates_;
  bool use_site_keyed_cache_ = false;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_
