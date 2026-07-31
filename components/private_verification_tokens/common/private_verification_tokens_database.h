// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_DATABASE_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_DATABASE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "sql/database.h"
#include "url/origin.h"

namespace private_verification_tokens {

struct TokenWithId {
  TokenWithId(int64_t id, PrivateVerificationTokensToken token);
  TokenWithId(const TokenWithId&);
  TokenWithId& operator=(const TokenWithId&);
  TokenWithId(TokenWithId&&);
  TokenWithId& operator=(TokenWithId&&);
  ~TokenWithId();

  int64_t id;
  PrivateVerificationTokensToken token;
};

// Implements PVT database operations. Constructor detaches the object
// from the sequence it is created on. All functions verify they are executed in
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

  // Creates a SequenceBound instance of this class. All database operations,
  // including file creation and schema initialization, are performed lazily
  // on the first operation called on the returned object.
  static base::SequenceBound<PrivateVerificationTokensDatabase>
  CreateSequenceBound(scoped_refptr<base::SequencedTaskRunner> task_runner,
                      base::FilePath path_to_database);

  // Detaches the object from the sequence it is created on. This allows moving
  // this class's object to sequences other than the one on which it was
  // created. The constructor is public only to allow base::SequenceBound (or
  // std::make_unique in Create) to call it, but is protected from general use
  // by the PassKey.
  explicit PrivateVerificationTokensDatabase(
      base::PassKey<PrivateVerificationTokensDatabase>,
      std::unique_ptr<sql::Database> database,
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

  // Store given tokens in the database.
  bool StoreTokens(const std::vector<PrivateVerificationTokensToken>& tokens);

  // Returns a single unredeemed token for the given `issuer`, or
  // `std::nullopt` if none exist. Calling this successively without calling
  // `SetRedeemed()` on the returned token might return the same token.
  std::optional<TokenWithId> GetToken(const url::Origin& issuer);

  // Get one token from each distinct issuer.
  std::map<url::Origin, TokenWithId> GetTokensFromEach();

  // Delete all tokens that are marked as redeemed.
  bool DeleteRedeemedTokens();

  // Delete tokens filtered by creation time range [delete_begin, delete_end)
  // and a list of issuer origins. If `issuers` is std::nullopt all rows that
  // match the time criteria will be deleted regardless of their issuer column
  // value. If `issuers` is an empty vector, no tokens are removed.
  bool DeleteTokens(base::Time delete_begin,
                    base::Time delete_end,
                    base::optional_ref<const std::vector<url::Origin>> issuers);

  // Mark token with the given id as redeemed.
  bool SetRedeemed(int64_t token_id);

  const base::FilePath& PathToDatabase() const;

 private:
  bool EnsureDBInitialized() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool InitializeDB() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Creates a new sql::Database object with standard options.
  static std::unique_ptr<sql::Database> CreateSqlDatabase();
  bool InitializeSchema(bool is_retry)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool CreateSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Helper function that actually constructs and executes the delete query;
  // it is not directly exposed in order to allow a wrapper method to chunk
  // up the origin list to work around the sqlite-in-Chromium max placeholder
  // count.
  bool DeleteTokenBatch(base::Time delete_begin,
                        base::Time delete_end,
                        base::span<const url::Origin> issuers);
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  std::unique_ptr<sql::Database> database_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::FilePath path_to_database_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_DATABASE_H_
