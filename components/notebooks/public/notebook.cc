// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/public/notebook.h"

#include <utility>

namespace notebooks {

Notebook::Notebook(NotebookId id,
                   base::Time creation_time,
                   base::Time update_time)
    : id_(std::move(id)),
      creation_time_(creation_time),
      update_time_(update_time) {}

Notebook::~Notebook() = default;

Notebook::Notebook(const Notebook&) = default;
Notebook& Notebook::operator=(const Notebook&) = default;
Notebook::Notebook(Notebook&&) = default;
Notebook& Notebook::operator=(Notebook&&) = default;

Notebook& Notebook::SetUpdateTime(base::Time update_time) {
  update_time_ = update_time;
  return *this;
}

}  // namespace notebooks
