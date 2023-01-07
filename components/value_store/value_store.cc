// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/value_store.h"

#include <utility>

#include "base/check.h"

namespace value_store {

// Implementation of Status.

ValueStore::Status::Status() = default;

ValueStore::Status::Status(StatusCode code, const std::string& message)
    : Status(code, RESTORE_NONE, message) {}

ValueStore::Status::Status(StatusCode code,
                           BackingStoreRestoreStatus restore_status,
                           const std::string& message)
    : code(code), restore_status(restore_status), message(message) {}

ValueStore::Status::Status(Status&& other) = default;

ValueStore::Status::~Status() = default;

ValueStore::Status& ValueStore::Status::operator=(Status&& rhs) = default;

void ValueStore::Status::Merge(const Status& status) {
  if (code == OK)
    code = status.code;
  if (message.empty() && !status.message.empty())
    message = status.message;
  if (restore_status == RESTORE_NONE)
    restore_status = status.restore_status;
}

// Implementation of ReadResult.

ValueStore::ReadResult::ReadResult(base::Value::Dict settings, Status status)
    : settings_(std::move(settings)), status_(std::move(status)) {}

ValueStore::ReadResult::ReadResult(Status status)
    : status_(std::move(status)) {}

ValueStore::ReadResult::ReadResult(ReadResult&& other) = default;

ValueStore::ReadResult::~ReadResult() = default;

ValueStore::ReadResult& ValueStore::ReadResult::operator=(ReadResult&& rhs) =
    default;

// Implementation of WriteResult.

ValueStore::WriteResult::WriteResult(ValueStoreChangeList changes,
                                     Status status)
    : changes_(std::move(changes)), status_(std::move(status)) {}

ValueStore::WriteResult::WriteResult(Status status)
    : status_(std::move(status)) {}

ValueStore::WriteResult::WriteResult(WriteResult&& other) = default;

ValueStore::WriteResult::~WriteResult() = default;

ValueStore::WriteResult& ValueStore::WriteResult::operator=(WriteResult&& rhs) =
    default;

}  // namespace value_store
