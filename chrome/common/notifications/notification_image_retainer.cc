// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/notifications/notification_image_retainer.h"

#include <algorithm>
#include <set>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "chrome/common/chrome_paths.h"
#include "ui/gfx/image/image.h"

namespace {

constexpr base::FilePath::CharType kImageRoot[] =
    FILE_PATH_LITERAL("Notification Resources");

// How long to keep the temp files before deleting them. The formula for picking
// the delay is t * (n + 1), where t is the default on-screen display time for
// an Action Center notification (6 seconds) and n is the number of
// notifications that can be shown on-screen at once (1).
constexpr base::TimeDelta kDeletionDelay = base::Seconds(12);

// Returns the temporary directory within the user data directory. The regular
// temporary directory is not used to minimize the risk of files getting deleted
// by accident. It is also not profile-bound because the notification bridge
// is profile-agnostic.
base::FilePath DetermineImageDirectory() {
  base::FilePath data_dir;
  bool success = base::PathService::Get(chrome::DIR_USER_DATA, &data_dir);
  DCHECK(success);
  return data_dir.Append(kImageRoot);
}

// Returns the full paths to all immediate file and directory children of |dir|,
// excluding those present in |registered_names|.
std::vector<base::FilePath> GetFilesFromPrevSessions(
    const base::FilePath& dir,
    const std::set<base::FilePath>& registered_names) {
  // |dir| may have sub-dirs, created by the old implementation.
  base::FileEnumerator file_enumerator(
      dir, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      FILE_PATH_LITERAL("*"));
  std::vector<base::FilePath> files;

  for (base::FilePath current = file_enumerator.Next(); !current.empty();
       current = file_enumerator.Next()) {
    // Exclude any new file created in this session.
    if (!base::Contains(registered_names, current.BaseName()))
      files.push_back(std::move(current));
  }

  return files;
}

// Deletes files in |paths|.
void DeleteFiles(std::vector<base::FilePath> paths) {
  // |file_path| can be a directory, created by the old implementation, so
  // delete it recursively.
  for (const auto& file_path : paths)
    base::DeletePathRecursively(file_path);
}

}  // namespace

NotificationImageRetainer::NotificationImageRetainer(
    scoped_refptr<base::SequencedTaskRunner> deletion_task_runner,
    const base::TickClock* tick_clock)
    : deletion_task_runner_(std::move(deletion_task_runner)),
      image_dir_(DetermineImageDirectory()),
      tick_clock_(tick_clock),
      deletion_timer_(tick_clock) {
  DCHECK(deletion_task_runner_);
  DCHECK(tick_clock);

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NotificationImageRetainer::NotificationImageRetainer()
    : NotificationImageRetainer(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
          base::DefaultTickClock::GetInstance()) {}

NotificationImageRetainer::~NotificationImageRetainer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NotificationImageRetainer::CleanupFilesFromPrevSessions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Store all file names from registered_images in an ordered set for quick
  // search.
  std::set<base::FilePath> registered_names;
  for (const auto& pair : registered_images_)
    registered_names.insert(pair.first);

  std::vector<base::FilePath> files =
      GetFilesFromPrevSessions(image_dir_, registered_names);

  // This method is run in an "after startup" task, so it is fine to directly
  // post the DeleteFiles task to the runner.
  if (!files.empty()) {
    deletion_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteFiles, std::move(files)));
  }
}

base::FilePath NotificationImageRetainer::RegisterTemporaryImage(
    const gfx::Image& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<base::RefCountedMemory> data = image.As1xPNGBytes();
  if (data->size() == 0)
    return base::FilePath();

  // Create the image directory. Since Chrome doesn't delete this directory
  // after showing notifications, this directory creation should happen exactly
  // once until Chrome is re-installed.
  if (!base::CreateDirectory(image_dir_))
    return base::FilePath();

  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(image_dir_, &temp_file))
    return base::FilePath();

  const base::TimeTicks now = tick_clock_->NowTicks();
  DCHECK(registered_images_.empty() || now >= registered_images_.back().second);
  registered_images_.emplace_back(temp_file.BaseName(), now);

  // At this point, a temp file is already created. We need to clean it up even
  // if it fails to write the image data to this file.
  bool data_write_success = base::WriteFile(temp_file, *data);

  // Start the timer if it hasn't to delete the expired files in batch. This
  // avoids creating a deletion task for each file, otherwise the overhead can
  // be large when there is a steady stream of notifications coming rapidly.
  if (!deletion_timer_.IsRunning()) {
    deletion_timer_.Start(
        FROM_HERE, kDeletionDelay,
        base::BindRepeating(&NotificationImageRetainer::DeleteExpiredFiles,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  return data_write_success ? temp_file : base::FilePath();
}

base::OnceClosure NotificationImageRetainer::GetCleanupTask() {
  return base::BindOnce(
      &NotificationImageRetainer::CleanupFilesFromPrevSessions,
      weak_ptr_factory_.GetWeakPtr());
}

void NotificationImageRetainer::DeleteExpiredFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!registered_images_.empty());

  // Find the first file that should not be deleted.
  const base::TimeTicks then = tick_clock_->NowTicks() - kDeletionDelay;
  const auto end =
      std::upper_bound(registered_images_.begin(), registered_images_.end(),
                       std::make_pair(base::FilePath(), then),
                       [](const NameAndTime& a, const NameAndTime& b) {
                         return a.second < b.second;
                       });
  if (end == registered_images_.begin())
    return;  // Nothing to delete yet.

  // Ship the files to be deleted off to the deletion task runner.
  std::vector<base::FilePath> files_to_delete;
  files_to_delete.reserve(end - registered_images_.begin());
  for (auto iter = registered_images_.begin(); iter < end; ++iter)
    files_to_delete.push_back(image_dir_.Append(iter->first));

  deletion_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteFiles, std::move(files_to_delete)));

  // Erase the items to be deleted from registered_images_.
  registered_images_.erase(registered_images_.begin(), end);

  // Stop the recurring timer if all files have been deleted.
  if (registered_images_.empty())
    deletion_timer_.Stop();
}
