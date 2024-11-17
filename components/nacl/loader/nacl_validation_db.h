// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_VALIDATION_DB_H_
#define COMPONENTS_NACL_LOADER_NACL_VALIDATION_DB_H_

#include <string>

class NaClValidationDB {
 public:
  NaClValidationDB() = default;

  NaClValidationDB(const NaClValidationDB&) = delete;
  NaClValidationDB& operator=(const NaClValidationDB&) = delete;

  virtual ~NaClValidationDB() = default;

  virtual bool QueryKnownToValidate(const std::string& signature) = 0;
  virtual void SetKnownToValidate(const std::string& signature) = 0;
};

#endif  // COMPONENTS_NACL_LOADER_NACL_VALIDATION_DB_H_
