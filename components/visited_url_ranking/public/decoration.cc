// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/decoration.h"

namespace visited_url_ranking {

Decoration::Decoration(DecorationType decoration_type)
    : type(decoration_type) {}

Decoration::Decoration(const Decoration&) = default;

}  // namespace visited_url_ranking
