// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_MODEL_LOADER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_MODEL_LOADER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"

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
  using LoadCallback =
      base::OnceCallback<void(std::unique_ptr<BookmarkLoadDetails>)>;
  // Creates the ModelLoader, and schedules loading on
  // |load_sequenced_task_runner|. |callback| is run once loading
  // completes (on the main thread).
  static scoped_refptr<ModelLoader> Create(
      const base::FilePath& profile_path,
      base::SequencedTaskRunner* load_sequenced_task_runner,
      std::unique_ptr<BookmarkLoadDetails> details,
      LoadCallback callback);

  // Blocks until loaded. This is intended for usage on a thread other than
  // the main thread.
  void BlockTillLoaded();

  // Returns null until the model has loaded. Use BlockTillLoaded() to ensure
  // this returns non-null.
  HistoryBookmarkModel* history_bookmark_model() {
    return history_bookmark_model_.get();
  }

 private:
  friend class base::RefCountedThreadSafe<ModelLoader>;
  ModelLoader();
  ~ModelLoader();

  // Performs the load on a background thread.
  void DoLoadOnBackgroundThread(
      const base::FilePath& profile_path,
      bool emit_experimental_uma,
      scoped_refptr<base::SequencedTaskRunner> main_sequenced_task_runner,
      std::unique_ptr<BookmarkLoadDetails> details,
      LoadCallback callback);

  void OnFinishedLoad(std::unique_ptr<BookmarkLoadDetails> details,
                      LoadCallback callback);

  scoped_refptr<HistoryBookmarkModel> history_bookmark_model_;

  // Signaled once loading completes.
  base::WaitableEvent loaded_signal_;

  DISALLOW_COPY_AND_ASSIGN(ModelLoader);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_MODEL_LOADER_H_
