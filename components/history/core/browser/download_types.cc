// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/download_types.h"

#include <ostream>

#include "base/logging.h"
#include "components/history/core/browser/download_constants.h"

namespace history {

DownloadState IntToDownloadState(int state) {
  switch (static_cast<DownloadState>(state)) {
    case DownloadState::IN_PROGRESS:
    case DownloadState::COMPLETE:
    case DownloadState::CANCELLED:
    case DownloadState::INTERRUPTED:
      return static_cast<DownloadState>(state);

    case DownloadState::INVALID:
    case DownloadState::BUG_140687:
      NOTREACHED();
      return DownloadState::INVALID;
  }
  NOTREACHED();
  return DownloadState::INVALID;
}

int DownloadStateToInt(DownloadState state) {
  DCHECK_NE(state, DownloadState::INVALID);
  return static_cast<int>(state);
}

std::ostream& operator<<(std::ostream& stream, DownloadState state) {
  switch (state) {
    case DownloadState::INVALID:
      return stream << "history::DownloadState::COMPLETE";
    case DownloadState::IN_PROGRESS:
      return stream << "history::DownloadState::IN_PROGRESS";
    case DownloadState::COMPLETE:
      return stream << "history::DownloadState::COMPLETE";
    case DownloadState::CANCELLED:
      return stream << "history::DownloadState::CANCELLED";
    case DownloadState::BUG_140687:
      return stream << "history::DownloadState::BUG_140687";
    case DownloadState::INTERRUPTED:
      return stream << "history::DownloadState::INTERRUPTED";
  }
  NOTREACHED();
  return stream;
}

DownloadDangerType IntToDownloadDangerType(int danger_type) {
  switch (static_cast<DownloadDangerType>(danger_type)) {
    case DownloadDangerType::NOT_DANGEROUS:
    case DownloadDangerType::DANGEROUS_FILE:
    case DownloadDangerType::DANGEROUS_URL:
    case DownloadDangerType::DANGEROUS_CONTENT:
    case DownloadDangerType::MAYBE_DANGEROUS_CONTENT:
    case DownloadDangerType::UNCOMMON_CONTENT:
    case DownloadDangerType::USER_VALIDATED:
    case DownloadDangerType::DANGEROUS_HOST:
    case DownloadDangerType::POTENTIALLY_UNWANTED:
    case DownloadDangerType::WHITELISTED_BY_POLICY:
    case DownloadDangerType::ASYNC_SCANNING:
    case DownloadDangerType::BLOCKED_PASSWORD_PROTECTED:
    case DownloadDangerType::BLOCKED_TOO_LARGE:
    case DownloadDangerType::SENSITIVE_CONTENT_WARNING:
    case DownloadDangerType::SENSITIVE_CONTENT_BLOCK:
    case DownloadDangerType::DEEP_SCANNED_SAFE:
    case DownloadDangerType::DEEP_SCANNED_OPENED_DANGEROUS:
      return static_cast<DownloadDangerType>(danger_type);

    case DownloadDangerType::INVALID:
      NOTREACHED();
      return DownloadDangerType::INVALID;
  }
  NOTREACHED();
  return DownloadDangerType::INVALID;
}

int DownloadDangerTypeToInt(DownloadDangerType danger_type) {
  DCHECK_NE(danger_type, DownloadDangerType::INVALID);
  return static_cast<int>(danger_type);
}

std::ostream& operator<<(std::ostream& stream, DownloadDangerType danger_type) {
  switch (danger_type) {
    case DownloadDangerType::INVALID:
      return stream << "history::DownloadDangerType::INVALID";
    case DownloadDangerType::NOT_DANGEROUS:
      return stream << "history::DownloadDangerType::NOT_DANGEROUS";
    case DownloadDangerType::DANGEROUS_FILE:
      return stream << "history::DownloadDangerType::DANGEROUS_FILE";
    case DownloadDangerType::DANGEROUS_URL:
      return stream << "history::DownloadDangerType::DANGEROUS_URL";
    case DownloadDangerType::DANGEROUS_CONTENT:
      return stream << "history::DownloadDangerType::DANGEROUS_CONTENT";
    case DownloadDangerType::MAYBE_DANGEROUS_CONTENT:
      return stream << "history::DownloadDangerType::MAYBE_DANGEROUS_CONTENT";
    case DownloadDangerType::UNCOMMON_CONTENT:
      return stream << "history::DownloadDangerType::UNCOMMON_CONTENT";
    case DownloadDangerType::USER_VALIDATED:
      return stream << "history::DownloadDangerType::USER_VALIDATED";
    case DownloadDangerType::DANGEROUS_HOST:
      return stream << "history::DownloadDangerType::DANGEROUS_HOST";
    case DownloadDangerType::POTENTIALLY_UNWANTED:
      return stream << "history::DownloadDangerType::POTENTIALLY_UNWANTED";
    case DownloadDangerType::WHITELISTED_BY_POLICY:
      return stream << "history::DownloadDangerType::WHITELISTED_BY_POLICY";
    case DownloadDangerType::ASYNC_SCANNING:
      return stream << "history::DownloadDangerType::ASYNC_SCANNING";
    case DownloadDangerType::BLOCKED_PASSWORD_PROTECTED:
      return stream
             << "history::DownloadDangerType::BLOCKED_PASSWORD_PROTECTED";
    case DownloadDangerType::BLOCKED_TOO_LARGE:
      return stream << "history::DownloadDangerType::BLOCKED_TOO_LARGE";
    case DownloadDangerType::SENSITIVE_CONTENT_WARNING:
      return stream << "history::DownloadDangerType::SENSITIVE_CONTENT_WARNING";
    case DownloadDangerType::SENSITIVE_CONTENT_BLOCK:
      return stream << "history::DownloadDangerType::SENSITIVE_CONTENT_BLOCK";
    case DownloadDangerType::DEEP_SCANNED_SAFE:
      return stream << "history::DownloadDangerType::DEEP_SCANNED_SAFE";
    case DownloadDangerType::DEEP_SCANNED_OPENED_DANGEROUS:
      return stream
             << "history::DownloadDangerType::DEEP_SCANNED_OPENED_DANGEROUS";
  }
  NOTREACHED();
  return stream;
}

DownloadInterruptReason IntToDownloadInterruptReason(int interrupt_reason) {
  return static_cast<DownloadInterruptReason>(interrupt_reason);
}

int DownloadInterruptReasonToInt(DownloadInterruptReason interrupt_reason) {
  return static_cast<int>(interrupt_reason);
}

const DownloadId kInvalidDownloadId = 0;

bool ConvertIntToDownloadId(int64_t id, DownloadId* out) {
  if (id <= static_cast<int64_t>(kInvalidDownloadId))
    return false;
  *out = static_cast<DownloadId>(id);
  return true;
}

int64_t DownloadIdToInt(DownloadId id) {
  DCHECK_NE(id, kInvalidDownloadId);
  return static_cast<int64_t>(id);
}

}  // namespace history
