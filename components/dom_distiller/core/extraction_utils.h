// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_EXTRACTION_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_EXTRACTION_UTILS_H_

#include <string>

#include "third_party/dom_distiller_js/dom_distiller.pb.h"

namespace dom_distiller {

// Returns the DomDistiller JavaScript web page distillation script with
// selected distallation `options`.
std::string GetDistillerScriptWithOptions(
    const dom_distiller::proto::DomDistillerOptions& options);

// Returns the Readability JavaScript web page distillation script.
std::string GetReadabilityDistillerScript();

// Returns the Javascript heuristic to determine if web pages are suitable for
// reader mode.
std::string GetReadabilityTriggeringScript();

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_EXTRACTION_UTILS_H_
