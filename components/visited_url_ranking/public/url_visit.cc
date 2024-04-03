// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

URLVisit::URLVisit() = default;

URLVisit::~URLVisit() = default;

URLVisit::URLVisit(URLVisit&& other) = default;

URLVisit& URLVisit::operator=(URLVisit&& other) = default;

}  // namespace visited_url_ranking
