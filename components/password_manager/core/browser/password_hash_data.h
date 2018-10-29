// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_HASH_DATA_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_HASH_DATA_H_

#include <stdint.h>

#include <string>

#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"

namespace password_manager {

struct PasswordHashData {
  PasswordHashData();
  PasswordHashData(const PasswordHashData& other);
  PasswordHashData(const std::string& username,
                   const base::string16& password,
                   bool force_update,
                   bool is_gaia_password = true);
  // Returns true iff |*this| represents the credential (|username|,
  // |password|), also with respect to whether it |is_gaia_password|.
  bool MatchesPassword(const std::string& username,
                       const base::string16& password,
                       bool is_gaia_password) const;

  std::string username;
  size_t length = 0;
  std::string salt;
  uint64_t hash = 0;
  bool force_update = false;
  bool is_gaia_password = true;
};

// Calculates 37 bits hash for a password. The calculation is based on a slow
// hash function. The running time is ~10^{-4} seconds on Desktop.
uint64_t CalculatePasswordHash(const base::StringPiece16& text,
                               const std::string& salt);

// If username is an email address, canonicalizes this email. Otherwise,
// append "@gmail.com" if it is gaia or returns |username| for non-Gaia account.
std::string CanonicalizeUsername(const std::string& username,
                                 bool is_gaia_account);

// Returns true if the two usernames the same after canonicalization.
bool AreUsernamesSame(const std::string& username1,
                      bool is_username1_gaia_account,
                      const std::string& username2,
                      bool is_username2_gaia_account);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_HASH_DATA_H_
