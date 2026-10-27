// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_OPTIONS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_OPTIONS_H_

#include <optional>
#include <string>

namespace dom_distiller {

// Configuration options for page distillation.
struct DistillerOptions {
  DistillerOptions();
  DistillerOptions(const DistillerOptions&);
  DistillerOptions& operator=(const DistillerOptions&);
  DistillerOptions(DistillerOptions&&);
  DistillerOptions& operator=(DistillerOptions&&);
  ~DistillerOptions();

  // A regular expression matching allowed video elements that can be preserved.
  // Tested against attributes of iframe and embed elements (such as `src` or
  // `data-src`) as well as their `innerHTML`. If std::nullopt or empty, the
  // default allowed video regex is used.
  std::optional<std::string> allowed_video_regex;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_OPTIONS_H_
