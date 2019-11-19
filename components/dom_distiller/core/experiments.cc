// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/experiments.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"

namespace dom_distiller {
DistillerHeuristicsType GetDistillerHeuristicsType() {
  // Get the field trial name first to ensure the experiment is initialized.
  const std::string group_name =
      base::FieldTrialList::FindFullName("ReaderModeUI");
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kReaderModeHeuristics);
  if (switch_value != "") {
    if (switch_value == switches::reader_mode_heuristics::kAdaBoost) {
      return DistillerHeuristicsType::ADABOOST_MODEL;
    }
    if (switch_value == switches::reader_mode_heuristics::kAllArticles) {
      return DistillerHeuristicsType::ALL_ARTICLES;
    }
    if (switch_value == switches::reader_mode_heuristics::kOGArticle) {
      return DistillerHeuristicsType::OG_ARTICLE;
    }
    if (switch_value == switches::reader_mode_heuristics::kAlwaysTrue) {
      return DistillerHeuristicsType::ALWAYS_TRUE;
    }
    if (switch_value == switches::reader_mode_heuristics::kNone) {
      return DistillerHeuristicsType::NONE;
    }
    NOTREACHED() << "Invalid value for " << switches::kReaderModeHeuristics;
  } else {
    if (base::StartsWith(group_name, "AdaBoost",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return DistillerHeuristicsType::ADABOOST_MODEL;
    }
    if (base::StartsWith(group_name, "AllArticles",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return DistillerHeuristicsType::ALL_ARTICLES;
    }
    if (base::StartsWith(group_name, "OGArticle",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return DistillerHeuristicsType::OG_ARTICLE;
    }
    if (base::StartsWith(group_name, "Disabled",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return DistillerHeuristicsType::NONE;
    }
  }
  return DistillerHeuristicsType::ADABOOST_MODEL;
}
}  // namespace dom_distiller
