// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_options.h"

namespace dom_distiller {

DistillerOptions::DistillerOptions() = default;
DistillerOptions::DistillerOptions(const DistillerOptions&) = default;
DistillerOptions& DistillerOptions::operator=(const DistillerOptions&) =
    default;
DistillerOptions::DistillerOptions(DistillerOptions&&) = default;
DistillerOptions& DistillerOptions::operator=(DistillerOptions&&) = default;
DistillerOptions::~DistillerOptions() = default;

}  // namespace dom_distiller
