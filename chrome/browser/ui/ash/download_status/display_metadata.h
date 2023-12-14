// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_METADATA_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_METADATA_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash::download_status {

// Lists the types of commands that can be performed on a displayed download.
// TODO(http://b/307353486): Implement all commands.
enum class CommandType {
  kCancel,
  kPause,
  kResume,
};

// The metadata to display a download command.
struct CommandInfo {
  CommandInfo(base::RepeatingClosure command_callback,
              const gfx::VectorIcon* icon,
              int text_id,
              CommandType type);
  CommandInfo(CommandInfo&&);
  CommandInfo& operator=(CommandInfo&&);
  ~CommandInfo();

  // The callback to run when this command is triggered.
  base::RepeatingClosure command_callback;

  // The command icon.
  raw_ptr<const gfx::VectorIcon> icon;

  // The identifier for the command text.
  int text_id;

  // The command type.
  CommandType type;
};

// The metadata used to display downloads.
// TODO(http://b/307347158): Fill `DisplayMetadata`.
struct DisplayMetadata {
  DisplayMetadata();
  DisplayMetadata(DisplayMetadata&&);
  DisplayMetadata& operator=(DisplayMetadata&&);
  ~DisplayMetadata();

  // Used to display download commands.
  std::vector<CommandInfo> command_infos;

  // The path to the file that bytes are actually written to during download.
  // NOTE: This path is different from the download target path.
  base::FilePath file_path;

  // The received bytes of download.
  std::optional<int64_t> received_bytes;

  // The text that provides additional details about the download.
  std::optional<std::u16string> secondary_text;

  // The primary text of the displayed download.
  std::optional<std::u16string> text;

  // The total bytes of download.
  std::optional<int64_t> total_bytes;
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_METADATA_H_
