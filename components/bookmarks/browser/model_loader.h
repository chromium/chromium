// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_MODEL_LOADER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_MODEL_LOADER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "components/bookmarks/browser/bookmark_client.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace bookmarks {

class BookmarkLoadDetails;
class HistoryBookmarkModel;

// ModelLoader is created by BookmarkModel to track loading of BookmarkModel.
// ModelLoader may be used on multiple threads. ModelLoader may outlive
// BookmarkModel.
class ModelLoader : public base::RefCountedThreadSafe<ModelLoader> {
 public:
  // Invoked when ModelLoader completes loading.
  using LoadCallback =
      base::OnceCallback<void(std::unique_ptr<BookmarkLoadDetails>)>;

  // Creates the ModelLoader, and schedules loading on a backend task runner.
  // `callback` is run once loading completes (on the main thread).
  // `local_or_syncable_file_path` must be non-empty and represents the
  // main (non-account) bookmarks, whereas `account_file_path` may be empty.
  static scoped_refptr<ModelLoader> Create(
      const base::FilePath& local_or_syncable_file_path,
      const base::FilePath& account_file_path,
      LoadManagedNodeCallback load_managed_node_callback,
      LoadCallback callback);

  ModelLoader(const ModelLoader&) = delete;
  ModelLoader& operator=(const ModelLoader&) = delete;

  // Blocks until loaded. This is intended for usage on a thread other than
  // the main thread.
  void BlockTillLoaded();

  // Returns null until the model has loaded. Use BlockTillLoaded() to ensure
  // this returns non-null.
  HistoryBookmarkModel* history_bookmark_model() {
    return history_bookmark_model_.get();
  }

  // Test-only factory function that creates a ModelLoader() that is initially
  // loaded.
  static scoped_refptr<ModelLoader> CreateForTest(
      LoadManagedNodeCallback load_managed_node_callback,
      BookmarkLoadDetails* details);

 private:
  friend class base::RefCountedThreadSafe<ModelLoader>;
  ModelLoader();
  ~ModelLoader();

  // Performs the load on a background thread.
  std::unique_ptr<BookmarkLoadDetails> DoLoadOnBackgroundThread(
      const base::FilePath& local_or_syncable_file_path,
      const base::FilePath& account_file_path,
      LoadManagedNodeCallback load_managed_node_callback);

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  scoped_refptr<HistoryBookmarkModel> history_bookmark_model_;

  // Signaled once loading completes.
  base::WaitableEvent loaded_signal_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_MODEL_LOADER_H_
