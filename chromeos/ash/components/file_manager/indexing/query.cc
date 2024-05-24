// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/query.h"

namespace ash::file_manager {

Query::Query(const std::vector<Term>& terms) : terms_(terms) {}

Query::Query(const Query& other) : terms_(other.terms_) {}

Query::~Query() = default;

}  // namespace ash::file_manager
