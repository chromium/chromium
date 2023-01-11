// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/distiller_ui_handle.h"

class GURL;

namespace dom_distiller {

class DistilledContentStore;
class DistillerFactory;
class DistillerPageFactory;
class TaskTracker;
class ViewerHandle;
class ViewRequestDelegate;

// Service for interacting with the Dom Distiller.
// Construction, destruction, and usage of this service must happen on the same
// thread. Callbacks will be called on that same thread.
class DomDistillerServiceInterface {
 public:
  virtual ~DomDistillerServiceInterface() = default;

  // Request to view an article by url.
  // Use CreateDefaultDistillerPage() to create a default |distiller_page|.
  // The provided |distiller_page| is only used if there is not already a
  // distillation task in progress for the given |url|.
  virtual std::unique_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate* delegate,
      std::unique_ptr<DistillerPage> distiller_page,
      const GURL& url) = 0;

  // Creates a default DistillerPage.
  virtual std::unique_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) = 0;
  virtual std::unique_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) = 0;

  // Returns the DistilledPagePrefs owned by the instance of
  // DomDistillerService.
  virtual DistilledPagePrefs* GetDistilledPagePrefs() = 0;

  // Returns the DistillerUIHandle owned by the instance of
  // DomDistillerService.
  virtual DistillerUIHandle* GetDistillerUIHandle() = 0;

  DomDistillerServiceInterface(const DomDistillerServiceInterface&) = delete;
  DomDistillerServiceInterface& operator=(const DomDistillerServiceInterface&) =
      delete;

 protected:
  DomDistillerServiceInterface() = default;
};

// Provide a view of the article list and ways of interacting with it.
class DomDistillerService : public DomDistillerServiceInterface {
 public:
  DomDistillerService(
      std::unique_ptr<DistillerFactory> distiller_factory,
      std::unique_ptr<DistillerPageFactory> distiller_page_factory,
      std::unique_ptr<DistilledPagePrefs> distilled_page_prefs,
      std::unique_ptr<DistillerUIHandle> distiller_ui_handle);
  ~DomDistillerService() override;

  // DomDistillerServiceInterface implementation.
  std::unique_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate* delegate,
      std::unique_ptr<DistillerPage> distiller_page,
      const GURL& url) override;
  std::unique_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) override;
  std::unique_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) override;
  DistilledPagePrefs* GetDistilledPagePrefs() override;
  DistillerUIHandle* GetDistillerUIHandle() override;
  base::WeakPtr<DomDistillerService> GetWeakPtr();

  bool HasTaskTrackerForTesting(const GURL& url) const;

 private:
  void CancelTask(TaskTracker* task);

  TaskTracker* CreateTaskTracker(const ArticleEntry& entry);

  TaskTracker* GetTaskTrackerForUrl(const GURL& url) const;

  // Gets the task tracker for the given |url|. If no appropriate
  // tracker exists, this will create one and put it in the |TaskTracker|
  // parameter passed into this function, initialize it, and add it to
  // |tasks_|. Return whether a |TaskTracker| needed to be created.
  bool GetOrCreateTaskTrackerForUrl(const GURL& url,
                                    TaskTracker** task_tracker);

  std::unique_ptr<DistilledContentStore> content_store_;
  std::unique_ptr<DistillerFactory> distiller_factory_;
  std::unique_ptr<DistillerPageFactory> distiller_page_factory_;
  std::unique_ptr<DistilledPagePrefs> distilled_page_prefs_;

  // An object for accessing chrome-specific UI controls including external
  // feedback and opening the distiller settings.
  std::unique_ptr<DistillerUIHandle> distiller_ui_handle_;

  typedef std::vector<std::unique_ptr<TaskTracker>> TaskList;
  TaskList tasks_;

  base::WeakPtrFactory<DomDistillerService> weak_ptr_factory_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_
