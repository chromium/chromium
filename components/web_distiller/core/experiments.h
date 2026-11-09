// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_EXPERIMENTS_H_
#define COMPONENTS_WEB_DISTILLER_CORE_EXPERIMENTS_H_

namespace web_distiller {
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.reader_mode
enum class DistillerHeuristicsType {
  NONE,
  OG_ARTICLE,
  ADABOOST_MODEL,
  ALL_ARTICLES,
  ALWAYS_TRUE,
};

DistillerHeuristicsType GetDistillerHeuristicsType();
}  // namespace web_distiller

#endif  // COMPONENTS_WEB_DISTILLER_CORE_EXPERIMENTS_H_
