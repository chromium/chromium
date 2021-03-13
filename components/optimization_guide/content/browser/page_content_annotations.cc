// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations.h"

PageContentAnnotations::PageContentAnnotations(
    const std::vector<std::pair<int, float>>& categories,
    float floc_protected_score)
    : categories_(categories), floc_protected_score_(floc_protected_score) {}

PageContentAnnotations::~PageContentAnnotations() = default;
PageContentAnnotations::PageContentAnnotations(
    const PageContentAnnotations& other) = default;
