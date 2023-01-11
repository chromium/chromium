// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_STORAGE_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_STORAGE_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/titled_url_index.h"

namespace base {
class SequencedTaskRunner;
}

namespace bookmarks {

class BookmarkModel;

// BookmarkStorage handles writing bookmark model to disk (as opposed to
// ModelLoader which takes care of loading).
//
// Internally BookmarkStorage uses BookmarkCodec to do the actual write.
class BookmarkStorage
    : public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  // How often the file is saved at most.
  static constexpr base::TimeDelta kSaveDelay = base::Milliseconds(2500);

  // Creates a BookmarkStorage for the specified model. The data will saved to a
  // file using the specified |file_path|. A backup file may be generated using
  // a name derived from |file_path| (appending suffix kBackupExtension). All
  // disk writes will be executed as a task in a backend task runner.
  BookmarkStorage(BookmarkModel* model, const base::FilePath& file_path);

  BookmarkStorage(const BookmarkStorage&) = delete;
  BookmarkStorage& operator=(const BookmarkStorage&) = delete;

  ~BookmarkStorage() override;

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // Notification the bookmark bar model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void BookmarkModelDeleted();

  // ImportantFileWriter::BackgroundDataSerializer implementation.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  // Returns whether there is still a pending write.
  bool HasScheduledSaveForTesting() const;

 private:
  // The state of the bookmark file backup. We lazily backup this file in order
  // to reduce disk writes until absolutely necessary. Will also leave the
  // backup unchanged if the browser starts & quits w/o changing bookmarks.
  enum BackupState {
    // No attempt has yet been made to backup the bookmarks file.
    BACKUP_NONE,
    // A request to backup the bookmarks file has been posted, but not yet
    // fulfilled.
    BACKUP_DISPATCHED,
    // The bookmarks file has been backed up (or at least attempted).
    BACKUP_ATTEMPTED
  };

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  // The model. The model is NULL once BookmarkModelDeleted has been invoked.
  raw_ptr<BookmarkModel> model_;

  // Sequenced task runner where disk writes will be performed at.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Helper to write bookmark data safely.
  base::ImportantFileWriter writer_;

  // The state of the backup file creation which is created lazily just before
  // the first scheduled save.
  bool backup_triggered_ = false;

  // Used to track the frequency of saves starting from the first save.
  base::TimeTicks last_scheduled_save_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_STORAGE_H_
