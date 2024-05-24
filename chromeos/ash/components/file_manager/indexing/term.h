// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TERM_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TERM_H_

#include <string>

#include "base/component_export.h"

namespace ash::file_manager {

// Represents a term that can be associated with a file or used to query for a
// file. An example term would be a label given to a file. If the file has
// label "starred" associated with it, it would be represented by the
// Term("label", u"starred") object. Other terms could be generated from the
// files' content, name, path, etc.
class COMPONENT_EXPORT(FILE_MANAGER) Term {
 public:
  Term(const std::string& field, const std::u16string& token);
  ~Term();

  // TODO(b:327535200): Reconsider copyability.
  Term(const Term&) = default;
  Term& operator=(const Term&) = default;

  const std::string& field() const { return field_; }
  const std::u16string token() const { return token_; }
  const std::string& token_bytes() const { return token_bytes_; }

 private:
  std::string field_;
  std::u16string token_;
  std::string token_bytes_;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TERM_H_
