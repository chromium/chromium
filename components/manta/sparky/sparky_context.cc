// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_context.h"

#include "components/manta/proto/sparky.pb.h"

namespace manta {

SparkyContext::SparkyContext(const proto::Turn& latest_turn)
    : latest_turn(latest_turn) {}

SparkyContext::SparkyContext(const proto::Turn& latest_turn,
                             const std::string& page_content)
    : latest_turn(latest_turn), page_content(page_content) {}

SparkyContext::~SparkyContext() = default;

SparkyContext::SparkyContext(const SparkyContext&) = default;
SparkyContext& SparkyContext::operator=(const SparkyContext&) = default;

}  // namespace manta
