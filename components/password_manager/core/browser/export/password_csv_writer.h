// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_CSV_WRITER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_CSV_WRITER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace password_manager {

struct CredentialUIEntry;

// Static-only class bundling together the API for serializing passwords into
// CSV format.
class PasswordCSVWriter {
 public:
  PasswordCSVWriter() = delete;
  PasswordCSVWriter(const PasswordCSVWriter&) = delete;
  PasswordCSVWriter& operator=(const PasswordCSVWriter&) = delete;

  // Creates a CSV representation of the credential stored in |credentials|.
  // Note that this loses all the metadata except for the origin, username and
  // password.
  static std::string SerializePasswords(
      const std::vector<CredentialUIEntry>& credentials);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_CSV_WRITER_H_
