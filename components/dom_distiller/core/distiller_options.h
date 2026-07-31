// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_OPTIONS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_OPTIONS_H_

#include "components/dom_distiller/core/readability_options.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"

namespace dom_distiller {

// Configuration options for page distillation, grouping options for both the
// DOM Distiller and Readability engines.
struct DistillerOptions {
  DistillerOptions();
  explicit DistillerOptions(proto::DomDistillerOptions dom_distiller_options);
  explicit DistillerOptions(ReadabilityOptions readability_options);
  ~DistillerOptions();

  // Options specific to the DOM Distiller engine.
  proto::DomDistillerOptions dom_distiller;
  // Options specific to the Readability engine.
  ReadabilityOptions readability;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_OPTIONS_H_
