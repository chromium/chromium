// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations.h"

PageContentAnnotations::PageContentAnnotations(
    const std::map<int, float>& categories,
    float sensitivity)
    : categories_(categories), sensitivity_(sensitivity) {}

PageContentAnnotations::~PageContentAnnotations() = default;
PageContentAnnotations::PageContentAnnotations(
    const PageContentAnnotations& other) = default;
