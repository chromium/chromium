// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_EXPERIMENTS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_EXPERIMENTS_H_

namespace dom_distiller {
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.dom_distiller
enum class DistillerHeuristicsType {
  NONE,
  OG_ARTICLE,
  ADABOOST_MODEL,
  ALL_ARTICLES,
  ALWAYS_TRUE,
};

DistillerHeuristicsType GetDistillerHeuristicsType();
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_EXPERIMENTS_H_
