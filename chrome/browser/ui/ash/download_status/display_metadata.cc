// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_metadata.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"

namespace ash::download_status {

// CommandInfo -----------------------------------------------------------------

CommandInfo::CommandInfo(base::RepeatingClosure command_callback,
                         const gfx::VectorIcon* icon,
                         int text_id,
                         CommandType type)
    : command_callback(std::move(command_callback)),
      icon(icon),
      text_id(text_id),
      type(type) {}

CommandInfo::CommandInfo(CommandInfo&&) = default;

CommandInfo& CommandInfo::operator=(CommandInfo&&) = default;

CommandInfo::~CommandInfo() = default;

// DisplayMetadata -------------------------------------------------------------

DisplayMetadata::DisplayMetadata() = default;

DisplayMetadata::DisplayMetadata(DisplayMetadata&&) = default;

DisplayMetadata& DisplayMetadata::operator=(DisplayMetadata&&) = default;

DisplayMetadata::~DisplayMetadata() = default;

// Progress --------------------------------------------------------------------

Progress::Progress()
    : Progress(/*received_bytes=*/std::nullopt,
               /*total_bytes=*/std::nullopt,
               /*complete=*/false,
               /*hidden=*/false) {}

Progress::Progress(const std::optional<int64_t>& received_bytes,
                   const std::optional<int64_t>& total_bytes,
                   bool complete,
                   bool hidden)
    : received_bytes_(received_bytes),
      total_bytes_(total_bytes),
      complete_(complete),
      hidden_(hidden) {
  const bool is_indeterminate = (!received_bytes_ || !total_bytes_);

  CHECK(is_indeterminate || received_bytes_ <= total_bytes_);
  CHECK_GE(received_bytes_.value_or(0), 0);
  CHECK_GE(total_bytes_.value_or(0), 0);

  // Check that for a completed download, `received_bytes` and `total_bytes`
  // have the same value. NOTE: When `received_bytes` and `total_bytes` have the
  // same value, `complete` can be false.
  if (complete_) {
    CHECK(!is_indeterminate);
    CHECK_EQ(received_bytes_.value(), total_bytes_.value());
  }
}

}  // namespace ash::download_status
