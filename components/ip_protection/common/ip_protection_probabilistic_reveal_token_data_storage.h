// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_DATA_STORAGE_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_DATA_STORAGE_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "sql/database.h"

namespace ip_protection {

struct TryGetProbabilisticRevealTokensOutcome;

// `IpProtectionProbabilisticRevealTokenDataStorage` can be created in the main
// thread (constructor does not have blocking IO operations) and caller is
// expected to post `StoreTokenOutcome()` off-main thread.
class IpProtectionProbabilisticRevealTokenDataStorage {
 public:
  explicit IpProtectionProbabilisticRevealTokenDataStorage(
      std::optional<base::FilePath> path_to_database);
  IpProtectionProbabilisticRevealTokenDataStorage(
      const IpProtectionProbabilisticRevealTokenDataStorage&) = delete;
  IpProtectionProbabilisticRevealTokenDataStorage& operator=(
      const IpProtectionProbabilisticRevealTokenDataStorage&) = delete;
  IpProtectionProbabilisticRevealTokenDataStorage(
      IpProtectionProbabilisticRevealTokenDataStorage&&) = delete;
  IpProtectionProbabilisticRevealTokenDataStorage& operator=(
      IpProtectionProbabilisticRevealTokenDataStorage&&) = delete;
  ~IpProtectionProbabilisticRevealTokenDataStorage();

  // Stores the outcome of a TryGetProbabilisticRevealTokens call to the given
  // database. If the existing database does not have the current version of the
  // schema, or if the database file is corrupted, the database will be razed
  // and re-initialized.
  void StoreTokenOutcome(TryGetProbabilisticRevealTokensOutcome outcome);

 private:
  bool EnsureDBInitialized() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool InitializeDB() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool InitializeSchema(bool is_retry = false)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool CreateSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);
  // A helper to encode string like absl::WebSafeBase64Escape().
  std::string base64url_encode(std::string);

  std::optional<base::FilePath> path_to_database_;

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_DATA_STORAGE_H_
