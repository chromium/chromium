// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_DATABASE_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_DATABASE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"
#include "sql/database.h"

namespace private_verification_tokens {

// Implements PVT database operations. Database object should be created
// off-main thread in a sequenced task runner. Constructor detaches the object
// from the sequence it is created. All functions verify they are executed in
// the correct sequence using checks. All DB operation functions (private
// functions of the class) use exclusive locks to access the sql::Database
// object `database_`. They are executed once when initializing the
// `database_`.
class PrivateVerificationTokensDatabase {
 public:
  // Check path and create a database object. It will return nullptr if the
  // path_to_database is empty.
  static std::unique_ptr<PrivateVerificationTokensDatabase> Create(
      base::FilePath path_to_database);
  PrivateVerificationTokensDatabase(const PrivateVerificationTokensDatabase&) =
      delete;
  PrivateVerificationTokensDatabase& operator=(
      const PrivateVerificationTokensDatabase&) = delete;
  PrivateVerificationTokensDatabase(PrivateVerificationTokensDatabase&&) =
      delete;
  PrivateVerificationTokensDatabase& operator=(
      PrivateVerificationTokensDatabase&&) = delete;

  ~PrivateVerificationTokensDatabase();

  // Store given keys in the database.
  bool StoreKeys(const std::vector<PrivateVerificationTokensPublicKey>& keys);

  // Remove all Keys for the given etld_plus_one.
  bool RemoveKeysFor(const std::string& etld_plus_one);

  // Remove the key with the given key_id for the specified etld_plus_one.
  bool RemoveKey(const std::string& etld_plus_one, uint32_t key_id);

  // Get all keys stored.
  std::vector<PrivateVerificationTokensPublicKey> GetKeys();

  const base::FilePath& PathToDatabase() const;

 private:
  // Detaches the object from the sequence it is created. This allows moving the
  // PVTDatabase object to sequences other than the one it is created.
  explicit PrivateVerificationTokensDatabase(
      std::unique_ptr<sql::Database> database,
      base::FilePath path_to_database);

  bool EnsureDBInitialized() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool InitializeDB() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool InitializeSchema(bool is_retry)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool CreateSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  std::unique_ptr<sql::Database> database_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::FilePath path_to_database_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_DATABASE_H_
