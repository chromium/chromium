// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_context.h"

namespace manta {

SparkyContext::SparkyContext(const std::vector<DialogTurn>& dialog)
    : dialog(dialog) {}

SparkyContext::SparkyContext(const std::vector<DialogTurn>& dialog,
                             const std::string& page_content)
    : dialog(dialog), page_content(page_content) {}

SparkyContext::~SparkyContext() = default;

SparkyContext::SparkyContext(const SparkyContext&) = default;
SparkyContext& SparkyContext::operator=(const SparkyContext&) = default;

}  // namespace manta
