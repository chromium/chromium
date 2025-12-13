// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model_load_waiter.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"

namespace bookmarks {

namespace {

// Self-owned observer, deletes itself on BookmarkModelLoaded() or
// BookmarkModelBeingDeleted().
class BookmarkModelLoadWaiter : public BaseBookmarkModelObserver {
 public:
  BookmarkModelLoadWaiter(BookmarkModel& model, base::OnceClosure callback) {
    CHECK(!model.loaded());
    CHECK(callback);

    callback_ = std::move(callback);
    scoped_observation_.Observe(&model);
  }

  ~BookmarkModelLoadWaiter() override = default;

  BookmarkModelLoadWaiter(const BookmarkModelLoadWaiter&) = delete;
  BookmarkModelLoadWaiter& operator=(const BookmarkModelLoadWaiter&) = delete;

  // BaseBookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override {
    // Handles scenarios in which callback_ destroys BookmarkModel.
    scoped_observation_.Reset();

    std::move(callback_).Run();
    delete this;
  }

  void BookmarkModelBeingDeleted() override {
    // If the BookmarkModel is destroyed before it finishes loading (e.g.,
    // during shutdown), delete the observer to prevent a memory leak. The
    // callback is not run in this case.
    delete this;
  }

  void BookmarkModelChanged() override {}

 private:
  // The callback to be executed once the model is loaded.
  base::OnceClosure callback_;
  base::ScopedObservation<BookmarkModel, BaseBookmarkModelObserver>
      scoped_observation_{this};
};

void RunCallbackIfModelExists(base::WeakPtr<BookmarkModel> model,
                              base::OnceClosure callback) {
  if (model) {
    std::move(callback).Run();
  }
}

}  // namespace

void ScheduleCallbackOnBookmarkModelLoad(BookmarkModel& model,
                                         base::OnceClosure callback) {
  CHECK(callback);

  if (model.loaded()) {
    // Posting a task to run the callback makes the function's behavior
    // consistent (always asynchronous) and avoids potential re-entrancy issues.
    // Callback is passed through a helper function to handle BookmarkModel
    // being destroyed between posting task and running callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RunCallbackIfModelExists, model.AsWeakPtr(),
                                  std::move(callback)));
    return;
  }

  new BookmarkModelLoadWaiter(model, std::move(callback));
}

}  // namespace bookmarks
