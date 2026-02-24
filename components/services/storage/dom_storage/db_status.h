// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DB_STATUS_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DB_STATUS_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/types/expected.h"

namespace storage {

// A database status code and optionally an error message. This status code
// may have originated from the database engine or from the Chromium code. See
// notes above `type_`.
class DbStatus {
 public:
  DbStatus();
  DbStatus(const DbStatus& rhs);
  DbStatus(DbStatus&& rhs) noexcept;
  ~DbStatus();

  DbStatus& operator=(const DbStatus& rhs);
  DbStatus& operator=(DbStatus&&) noexcept;

  // Create a success or error status.
  static DbStatus OK();
  static DbStatus NotFound(std::string_view msg);
  static DbStatus Corruption(std::string_view msg);
  static DbStatus NotSupported(std::string_view msg);
  static DbStatus IOError(std::string_view msg = {});
  static DbStatus InvalidArgument(std::string_view msg);
  static DbStatus DatabaseEngineCode(int code, std::string_view msg);

  // Returns true iff the status indicates the corresponding success or error.
  bool ok() const;
  bool IsNotFound() const;
  bool IsCorruption() const;

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  std::string ToString() const;

  // Logs the Type of this status to the given histogram name with the
  // appropriate suffix based on whether the database is in-memory or on-disk.
  void Log(std::string_view histogram_base, bool in_memory) const;

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(DomStorageDbStatusType)
  enum class Type {
    kOk = 0,

    // Something wasn't found.
    kNotFound = 1,

    // The database is in an inconsistent state.
    kCorruption = 2,

    kNotSupported = 3,

    // Generally speaking, indicates a programming error or unexpected state in
    // Chromium. For example, an invalid object store ID is sent as a parameter
    // over IPC.
    kInvalidArgument = 4,

    // Possibly transient read or write error.
    kIoError = 5,

    // An error reported by the database engine like SQLite.
    kDatabaseEngineCode = 6,

    kMaxValue = kDatabaseEngineCode,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:DomStorageDbStatusType)

  // Initializes `database_engine_code_`.  Sets `type_` to
  // `kDatabaseEngineCode`.
  DbStatus(int database_engine_code, std::string_view msg);

  // For all other types.  `type` must not be `kDatabaseEngineCode`.  Leaves
  // `database_engine_code_` unset.
  DbStatus(Type type, std::string_view msg);

  // The specific type of error. Note that the treatment of this is quite
  // inconsistent:
  // * sometimes it has semantic value, as in code branches based on
  //   `IsCorruption()`
  // * sometimes it's used for logging
  // * sometimes it's just ignored
  Type type_;

  std::string msg_;

  // Null unless `type_` is `kDatabaseEngineCode`.
  std::optional<int> database_engine_code_;
};

// Makes a common return value more concise. For this return type, "no error" is
// represented by returning a value for `T`, and the DbStatus should never be
// `ok()`.
template <typename T>
using StatusOr = base::expected<T, DbStatus>;

// One common way of returning an error from a function that does not otherwise
// return a value would be base::expected<void, DbStatus>, and that would allow
// us to make use of the `base::expected` macros such as RETURN_IF_ERROR.
// However, that would require updating tons of code, so we simply define
// similar macros.
#define DB_RETURN_IF_ERROR_AND_DO(expr, on_error) \
  {                                               \
    DbStatus _status = expr;                      \
    if (!_status.ok()) [[unlikely]] {             \
      on_error;                                   \
      return _status;                             \
    }                                             \
  }

#define DB_RETURN_IF_ERROR(expr) DB_RETURN_IF_ERROR_AND_DO(expr, {})

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DB_STATUS_H_
