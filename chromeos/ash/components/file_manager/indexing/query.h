// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_QUERY_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_QUERY_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/file_manager/indexing/term.h"

namespace ash::file_manager {

// Represents a parsed query.
class COMPONENT_EXPORT(FILE_MANAGER) Query {
 public:
  explicit Query(const std::vector<Term>& terms);
  ~Query();

  // TODO(b:327535200): Reconsider copyability.
  Query(const Query& query);
  Query& operator=(const Query& other) = default;

  const std::vector<Term>& terms() const { return terms_; }

 private:
  std::vector<Term> terms_;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_QUERY_H_
