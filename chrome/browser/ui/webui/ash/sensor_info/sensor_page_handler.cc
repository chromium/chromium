// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/sensor_info/sensor_page_handler.h"

#include <string>
#include <utility>

#include "ash/sensor_info/sensor_provider.h"
#include "ash/sensor_info/sensor_types.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

SensorPageHandler::SensorPageHandler(
    Profile* profile,
    SensorProvider* provider,
    mojo::PendingReceiver<sensor::mojom::PageHandler> receiver,
    base::FilePath out_file_path)
    : profile_(profile),
      provider_(provider),
      receiver_(this, std::move(receiver)),
      out_file_path_(std::move(out_file_path)) {
  provider_->AddObserver(this);
  provider_->EnableSensorReading();
}

SensorPageHandler::SensorPageHandler(
    Profile* profile,
    ash::SensorProvider* provider,
    mojo::PendingReceiver<sensor::mojom::PageHandler> receiver)
    : SensorPageHandler(
          profile,
          provider,
          std::move(receiver),
          GetDownloadsDirectory(profile).Append("sensor_info.txt")) {}

SensorPageHandler::~SensorPageHandler() {
  provider_->StopSensorReading();
  provider_->RemoveObserver(this);
  ResetFile();
}

base::FilePath SensorPageHandler::GetDownloadsDirectory(Profile* profile) {
  const DownloadPrefs* const prefs = DownloadPrefs::FromBrowserContext(profile);
  base::FilePath path = prefs->DownloadPath();
  // Gets the DownloadsDirectory. Checks whether the given 'path' points to a
  // non-local filesystem that requires special handling.
  if (file_manager::util::IsUnderNonNativeLocalPath(profile, path)) {
    path = prefs->GetDefaultDownloadDirectoryForProfile();
  }
  return path;
}

void SensorPageHandler::OnSensorUpdated(const ash::SensorUpdate& update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<sensor::mojom::SensorUpdateInfoPtr> mojom_updates;
  for (int sensor_type = 0;
       sensor_type < static_cast<int>(ash::SensorType::kSensorTypeCount) - 1;
       ++sensor_type) {
    auto source = static_cast<ash::SensorType>(sensor_type);
    auto source_update = update.get(source);
    if (!source_update.has_value()) {
      continue;
    }

    if (source == ash::SensorType::kLidAngle) {
      mojom_updates.push_back(
          sensor::mojom::SensorUpdateInfo::NewLidAngleUpdateInfo(
              sensor::mojom::LidAngleUpdateInfo::New(
                  static_cast<sensor::mojom::SensorType>(source),
                  source_update->x)));
    } else {
      mojom_updates.push_back(sensor::mojom::SensorUpdateInfo::NewUpdateInfo(
          sensor::mojom::NonLidAngleUpdateInfo::New(
              static_cast<sensor::mojom::SensorType>(source), source_update->x,
              source_update->y, source_update->z)));
    }
  }
  // If saving is on and output file is opened.
  if (state_ == State::kOpened && save_update_ == true) {
    auto sensor_callback = base::BindOnce(
        [](const ash::SensorUpdate& update, base::File* file_ptr) {
          std::string update_output = GenerateString(update, base::Time::Now());
          if (file_ptr->WriteAtCurrentPosAndCheck(base::span<uint8_t>(
                  reinterpret_cast<uint8_t*>(update_output.data()),
                  update_output.size()))) {
            LOG(ERROR) << "Write Unsuccessful.";
          }
        },
        update, out_file_.get());
    task_runner_->PostTask(FROM_HERE, std::move(sensor_callback));
  }
}

std::string SensorPageHandler::GenerateString(const ash::SensorUpdate& update,
                                              base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  std::string update_output = base::StringPrintf(
      "%04d-%02d-%02d %02d:%02d:%02d.%03d UTC\n", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);

  for (int sensor_type = 0;
       sensor_type < static_cast<int>(ash::SensorType::kSensorTypeCount);
       ++sensor_type) {
    auto source = static_cast<ash::SensorType>(sensor_type);
    auto source_update = update.get(source);
    if (!source_update.has_value()) {
      update_output += "None.\n";
      continue;
    }

    if (source == ash::SensorType::kLidAngle) {
      update_output += base::StringPrintf("%.2f\n", source_update->x);
    } else {
      update_output += base::StringPrintf("%.2f %.2f %.2f\n", source_update->x,
                                          source_update->y, source_update->z);
    }
  }
  return update_output;
}

void SensorPageHandler::StartRecordingUpdate() {
  CHECK(!save_update_) << "Impossible state: open file already during open.";
  CHECK_NE(state_, State::kOpened)
      << "Impossible state: open file already during open.";
  save_update_ = true;
  if (state_ == State::kOpening) {
    // That there's an active file open in progress, and we should just switch
    // states and let it proceed.
    return;
  }
  state_ = State::kOpening;
  auto open_callback = base::BindOnce(
      [](const base::FilePath& filename) -> std::unique_ptr<base::File> {
        auto out_file = std::make_unique<base::File>(
            filename, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
        if (!out_file->IsValid()) {
          LOG(ERROR) << "Failed to create diagnostics log file " << filename;
          return nullptr;
        }
        return out_file;
      },
      out_file_path_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(open_callback),
      base::BindOnce(&SensorPageHandler::OnFileOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SensorPageHandler::OnFileOpened(std::unique_ptr<base::File> opened_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kOpening)
      << "Impossible state: open file state should be kOpening.";
  out_file_ = std::move(opened_file);
  if (save_update_) {
    // Open received when 'save_update_' is true, that's fine. Lets set to open
    // and go.
    state_ = State::kOpened;
  } else {
    ResetFile();
  }
}

void SensorPageHandler::StopRecordingUpdate() {
  CHECK(save_update_) << "Impossible state: close file when already closed.";
  CHECK_NE(state_, State::kStopped)
      << "Impossible state: close file when already closed.";
  save_update_ = false;
  if (state_ == State::kOpening) {
    // a file open is in progress, let's switch to stopping state and leave.
    return;
  }
  ResetFile();
}

bool SensorPageHandler::CheckOutFileForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return out_file_.get();
}

void SensorPageHandler::ResetFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kStopped;
  if (!out_file_) {
    return;
  }
  task_runner_->PostTask(FROM_HERE,
                         base::DoNothingWithBoundArgs(std::move(out_file_)));
}

}  // namespace ash
