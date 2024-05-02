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
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash::download_status {

// Lists the types of commands that can be performed on a displayed download.
enum class CommandType {
  kCancel,
  kCopyToClipboard,
  kEditWithMediaApp,
  kOpenFile,
  kOpenWithMediaApp,
  kPause,
  kResume,
  kShowInBrowser,
  kShowInFolder,
  kViewDetailsInBrowser,
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

// Indicates a download's progress. A progress is indeterminate if either
// `received_bytes` or `total_bytes` is unknown.
class Progress {
 public:
  Progress();

  // Creates an instance for the specified progress attributes. NOTE:
  // 1. The values of `received_bytes` and `total_bytes`, if any, must be
  //    non-negative.
  // 2. `received_bytes` must not be greater than `total_bytes` unless the
  //     progress is indeterminate.
  // 3. When `complete` is true, `received_bytes` and `total_bytes` must have
  //    values and be equal.
  Progress(const std::optional<int64_t>& received_bytes,
           const std::optional<int64_t>& total_bytes,
           bool complete,
           bool hidden);

  bool complete() const { return complete_; }

  bool hidden() const { return hidden_; }

  const std::optional<int64_t>& received_bytes() const {
    return received_bytes_;
  }

  const std::optional<int64_t>& total_bytes() const { return total_bytes_; }

 private:
  std::optional<int64_t> received_bytes_;
  std::optional<int64_t> total_bytes_;
  bool complete_;

  // True if progress data should not be visibly represented.
  bool hidden_;
};

// The metadata used to display downloads.
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

  // Points to valid dark/light mode download status icons. Null if either icon
  // is missing or invalid.
  crosapi::mojom::DownloadStatusIconsPtr icons;

  // A nullable image that represents the underlying download.
  gfx::ImageSkia image;

  // Indicates the progress of the underlying download.
  Progress progress;

  // The text that provides additional details about the download.
  std::optional<std::u16string> secondary_text;

  // The primary text of the displayed download.
  std::optional<std::u16string> text;
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_METADATA_H_
