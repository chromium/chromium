// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_PAGE_FEATURES_H_
#define COMPONENTS_DOM_DISTILLER_CORE_PAGE_FEATURES_H_

#include <vector>

#include "base/values.h"
#include "url/gurl.h"

class GURL;

namespace dom_distiller {

// The length of the derived features vector.
extern int kDerivedFeaturesCount;

// The distillable page detector is a model trained on a list of numeric
// features derived from features of a webpage (like body's number of elements
// ). This derives the numeric features form a set of core features.
//
// Note: It is crucial that these features are derived in the same way and are
// in the same order as in the training pipeline. See //heuristics/distillable
// in the external DomDistiller repo.
std::vector<double> CalculateDerivedFeatures(bool isOGArticle,
                                             const GURL& url,
                                             double numElements,
                                             double numAnchors,
                                             double numForms,
                                             const std::string& innerText,
                                             const std::string& textContent,
                                             const std::string& innerHTML);

// Calculates the derived features from the JSON value as returned by the
// javascript core feature extraction.
std::vector<double> CalculateDerivedFeaturesFromJSON(
    const base::Value* stringified_json);

std::vector<double> CalculateDerivedFeatures(bool openGraph,
                                             const GURL& url,
                                             unsigned elementCount,
                                             unsigned anchorCount,
                                             unsigned formCount,
                                             double mozScore,
                                             double mozScoreAllSqrt,
                                             double mozScoreAllLinear);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_PAGE_FEATURES_H_
