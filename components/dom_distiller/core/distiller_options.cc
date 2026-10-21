// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_options.h"

#include <utility>

namespace dom_distiller {

DistillerOptions::DistillerOptions(ReadabilityOptions readability_options)
    : readability(std::move(readability_options)) {}

DistillerOptions::~DistillerOptions() = default;

}  // namespace dom_distiller
