// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/answerer.h"

namespace history_embeddings {

Answerer::Context::Context() = default;

Answerer::Context::Context(const Context& other)
    : url_passages_map(other.url_passages_map) {}

Answerer::Context::~Context() = default;

}  // namespace history_embeddings
