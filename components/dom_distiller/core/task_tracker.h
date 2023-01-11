// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_TASK_TRACKER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_TASK_TRACKER_H_

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/dom_distiller/core/article_distillation_update.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"

class GURL;

namespace dom_distiller {

class DistilledArticleProto;
class DistilledContentStore;

// A handle to a request to view a DOM distiller entry or URL. The request will
// be cancelled when the handle is destroyed.
class ViewerHandle {
 public:
  using CancelCallback = base::OnceCallback<void()>;
  explicit ViewerHandle(CancelCallback callback);
  ~ViewerHandle();

  ViewerHandle(const ViewerHandle&) = delete;
  ViewerHandle& operator=(const ViewerHandle&) = delete;

 private:
  CancelCallback cancel_callback_;
};

// Interface for a DOM distiller entry viewer. Implement this to make a view
// request and receive the data for an entry when it becomes available.
class ViewRequestDelegate : public base::CheckedObserver {
 public:
  ~ViewRequestDelegate() override = default;

  // Called when the distilled article contents are available. The
  // DistilledArticleProto is owned by a TaskTracker instance and is invalidated
  // when the corresponding ViewerHandle is destroyed (or when the
  // DomDistillerService is destroyed).
  virtual void OnArticleReady(const DistilledArticleProto* article_proto) = 0;

  // Called when an article that is currently under distillation is updated.
  virtual void OnArticleUpdated(ArticleDistillationUpdate article_update) = 0;
};

// A TaskTracker manages the various tasks related to viewing, saving,
// distilling, and fetching an article's distilled content.
//
// There are two sources of distilled content, a Distiller and the BlobFetcher.
// At any time, at most one of each of these will be in-progress (if one
// finishes, the other will be cancelled).
//
// There are also two consumers of that content, a view request and a save
// request. There is at most one save request (i.e. we are either adding this to
// the reading list or we aren't), and may be multiple view requests. When
// the distilled content is ready, each of these requests will be notified.
//
// A view request is cancelled by deleting the corresponding ViewerHandle. Once
// all view requests are cancelled (and the save callback has been called if
// appropriate) the cancel callback will be called.
//
// After creating a TaskTracker, a consumer of distilled content should be added
// and at least one of the sources should be started.
class TaskTracker {
 public:
  using CancelCallback = base::OnceCallback<void(TaskTracker*)>;
  using SaveCallback = base::OnceCallback<
      void(const ArticleEntry&, const DistilledArticleProto*, bool)>;

  TaskTracker(const ArticleEntry& entry,
              CancelCallback callback,
              DistilledContentStore* content_store);
  ~TaskTracker();

  // |factory| will not be stored after this call.
  void StartDistiller(DistillerFactory* factory,
                      std::unique_ptr<DistillerPage> distiller_page);
  void StartBlobFetcher();

  void AddSaveCallback(SaveCallback callback);

  void CancelSaveCallbacks();

  // The ViewerHandle should be destroyed before the ViewRequestDelegate.
  std::unique_ptr<ViewerHandle> AddViewer(ViewRequestDelegate* delegate);

  const std::string& GetEntryId() const;
  bool HasEntryId(const std::string& entry_id) const;
  bool HasUrl(const GURL& url) const;

  TaskTracker(const TaskTracker&) = delete;
  TaskTracker& operator=(const TaskTracker&) = delete;

 private:
  void OnArticleDistillationUpdated(
      const ArticleDistillationUpdate& article_update);

  void OnDistillerFinished(
      std::unique_ptr<DistilledArticleProto> distilled_article);
  void OnBlobFetched(bool success,
                     std::unique_ptr<DistilledArticleProto> distilled_article);

  void RemoveViewer(ViewRequestDelegate* delegate);

  void DistilledArticleReady(
      std::unique_ptr<DistilledArticleProto> distilled_article);

  // Posts a task to run DoSaveCallbacks with |distillation_succeeded|.
  void ScheduleSaveCallbacks(bool distillation_succeeded);

  // Runs all callbacks passing |distillation_succeeded| and clears them.
  void DoSaveCallbacks(bool distillation_succeeded);

  void AddDistilledContentToStore(const DistilledArticleProto& content);

  void NotifyViewersAndCallbacks();
  void NotifyViewer(ViewRequestDelegate* delegate);

  bool IsAnySourceRunning() const;
  void ContentSourceFinished();

  void CancelPendingSources();
  void MaybeCancel();

  CancelCallback cancel_callback_;

  raw_ptr<DistilledContentStore> content_store_;

  std::vector<SaveCallback> save_callbacks_;
  // A ViewRequestDelegate will be added to this list when a view request is
  // made and removed when the corresponding ViewerHandle is destroyed.
  base::ObserverList<ViewRequestDelegate> viewers_;

  std::unique_ptr<Distiller> distiller_;
  bool blob_fetcher_running_;

  ArticleEntry entry_;
  std::unique_ptr<DistilledArticleProto> distilled_article_;

  bool content_ready_;

  bool destruction_allowed_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<TaskTracker> weak_ptr_factory_{this};
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_TASK_TRACKER_H_
