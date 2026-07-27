// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOK_ID_H_
#define COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOK_ID_H_

#include <cstddef>

#include "base/types/strong_alias.h"
#include "base/uuid.h"

namespace notebooks {

// Strongly-typed alias for Notebook entity UUIDs.
using NotebookId = base::StrongAlias<class NotebookIdTag, base::Uuid>;

struct NotebookIdHash {
  size_t operator()(const NotebookId& id) const {
    return base::UuidHash()(id.value());
  }
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOK_ID_H_
