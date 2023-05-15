// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/annotator.h"

namespace browsing_topics {

Annotation::Annotation(const std::string& input) : input(input) {}
Annotation::Annotation(const Annotation& other) = default;
Annotation::~Annotation() = default;

}  // namespace browsing_topics
