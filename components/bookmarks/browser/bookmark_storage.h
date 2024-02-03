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

  // Determines which subset of permanent folders need to be written to JSON.
  enum PermanentNodeSelection {
    kSelectLocalOrSyncableNodes,
    kSelectAccountNodes,
  };

  // Creates a BookmarkStorage for the specified model. `model` must not be null
  // and must outlive this object. The data will saved to a file using the
  // specified `file_path`. This data includes the set of permanent nodes
  // determined by `permanent_node_selection`.
  //
  // A backup file may be generated using a name derived from `file_path`
  // (appending suffix kBackupExtension).
  //
  // All disk writes will be executed as a task in a backend task runner.
  BookmarkStorage(const BookmarkModel* model,
                  PermanentNodeSelection permanent_node_selection,
                  const base::FilePath& file_path);

  BookmarkStorage(const BookmarkStorage&) = delete;
  BookmarkStorage& operator=(const BookmarkStorage&) = delete;

  // Upon destruction, if there is a pending save, it is saved immediately.
  ~BookmarkStorage() override;

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // ImportantFileWriter::BackgroundDataSerializer implementation.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  // Returns whether there is still a pending write.
  bool HasScheduledSaveForTesting() const;

  // If there is a pending write, performs it immediately.
  void SaveNowIfScheduledForTesting();

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

  // If there is a pending write, it performs it immediately.
  void SaveNowIfScheduled();

  const raw_ptr<const BookmarkModel> model_;

  // Sequenced task runner where disk writes will be performed at.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  const PermanentNodeSelection permanent_node_selection_;

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
