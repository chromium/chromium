// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_metadata.h"

#include <utility>

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

}  // namespace ash::download_status
