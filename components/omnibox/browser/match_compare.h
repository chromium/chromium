// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MATCH_COMPARE_H_
#define COMPONENTS_OMNIBOX_BROWSER_MATCH_COMPARE_H_

#include "components/omnibox/browser/omnibox_field_trial.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using PageClassification = metrics::OmniboxEventProto::PageClassification;

// This class implements a special version of AutocompleteMatch::MoreRelevant
// that allows matches of particular types to be demoted in AutocompleteResult.
template <class Match>
class CompareWithDemoteByType {
 public:
  CompareWithDemoteByType(PageClassification page_classification) {
    OmniboxFieldTrial::GetDemotionsByType(page_classification, &demotions_);
  }

  // Returns the relevance score of |match| demoted appropriately by
  // |demotions_by_type_|.
  int GetDemotedRelevance(const Match& match) const {
    auto demotion_it = demotions_.find(match.type);
    return (demotion_it == demotions_.end())
               ? match.relevance
               : (match.relevance * demotion_it->second);
  }

  // Comparison function.
  bool operator()(const Match& elem1, const Match& elem2) {
    // Compute demoted relevance scores for each match.
    const int demoted_relevance1 = GetDemotedRelevance(elem1);
    const int demoted_relevance2 = GetDemotedRelevance(elem2);
    if (demoted_relevance1 != demoted_relevance2) {
      // Greater relevance should come first.
      return demoted_relevance1 > demoted_relevance2;
    }
    // For equal-relevance matches, we sort alphabetically, so that providers
    // who return multiple elements at the same priority get a "stable" sort
    // across multiple updates.
    return elem1.contents < elem2.contents;
  }

 private:
  OmniboxFieldTrial::DemotionMultipliers demotions_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MATCH_COMPARE_H_
