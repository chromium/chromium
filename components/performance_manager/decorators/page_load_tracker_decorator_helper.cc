// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_load_tracker_decorator_helper.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace performance_manager {

namespace {

void NotifyPageLoadTrackerDecoratorOnPMSequence(content::WebContents* contents,
                                                void (*method)(PageNodeImpl*)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> node, void (*method)(PageNodeImpl*)) {
            if (node) {
              PageNodeImpl* page_node = PageNodeImpl::FromNode(node.get());
              method(page_node);
            }
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(contents),
          method));
}

}  // namespace

// Listens to content::WebContentsObserver notifications for a given WebContents
// and updates the PageLoadTracker accordingly. Destroys itself when the
// WebContents it observes is destroyed.
class PageLoadTrackerDecoratorHelper::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(content::WebContents* web_contents,
                               PageLoadTrackerDecoratorHelper* outer)
      : content::WebContentsObserver(web_contents),
        outer_(outer),
        prev_(nullptr),
        next_(outer->first_web_contents_observer_) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(web_contents);

    if (next_) {
      DCHECK(!next_->prev_);
      next_->prev_ = this;
    }
    outer_->first_web_contents_observer_ = this;

    // |web_contents| must not be loading when it starts being tracked by this
    // observer. Otherwise, loading state wouldn't be tracked correctly.
    DCHECK(!web_contents->ShouldShowLoadingUI());
  }

  WebContentsObserver(const WebContentsObserver&) = delete;
  WebContentsObserver& operator=(const WebContentsObserver&) = delete;

  ~WebContentsObserver() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  // content::WebContentsObserver:
  void DidStartLoading() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(web_contents()->IsLoading());

    // May be called spuriously when the `WebContents` is already loading.
    if (loading_state_ != LoadingState::kNotLoading) {
      return;
    }

    // Only observe top-level navigation to a different document.
    if (!web_contents()->ShouldShowLoadingUI()) {
      return;
    }

    loading_state_ = LoadingState::kWaitingForNavigation;
    NotifyPageLoadTrackerDecoratorOnPMSequence(
        web_contents(), &PageLoadTrackerDecorator::DidStartLoading);
  }

  void PrimaryPageChanged(content::Page& page) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Only observe top-level navigation that will show navigation UI.
    if (!web_contents()->ShouldShowLoadingUI())
      return;

    DCHECK(web_contents()->IsLoading());

    // Return if the state is already updated, since it can be called in
    // multiple times between DidStartLoading() and DidStopLoading().
    if (loading_state_ == LoadingState::kLoading)
      return;

    // There are a few cases where an ongoing navigation will get upgraded to
    // show loading ui without a DidStartLoading (e.g., if an iframe navigates,
    // then the top-level frame begins navigating before the iframe navigation
    // completes). If that happened, emulate the DidStartLoading now before
    // notifying PrimaryPageChanged.
    if (loading_state_ != LoadingState::kWaitingForNavigation) {
      NotifyPageLoadTrackerDecoratorOnPMSequence(
          web_contents(), &PageLoadTrackerDecorator::DidStartLoading);
    }

    loading_state_ = LoadingState::kLoading;
    NotifyPageLoadTrackerDecoratorOnPMSequence(
        web_contents(), &PageLoadTrackerDecorator::PrimaryPageChanged);
  }

  void DidStopLoading() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // The state can be |kNotLoading| if this isn't a top-level navigation to a
    // different document.
    if (loading_state_ == LoadingState::kNotLoading)
      return;

    loading_state_ = LoadingState::kNotLoading;

    NotifyPageLoadTrackerDecoratorOnPMSequence(
        web_contents(), &PageLoadTrackerDecorator::DidStopLoading);
  }

  void WebContentsDestroyed() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DetachAndDestroy();
  }

  // Removes the WebContentsObserver from the linked list and deletes it.
  void DetachAndDestroy() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (prev_) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(prev_->sequence_checker_);
      DCHECK_EQ(prev_->next_, this);
      prev_->next_ = next_;
    } else {
      DCHECK_EQ(outer_->first_web_contents_observer_, this);
      outer_->first_web_contents_observer_ = next_;
    }
    if (next_) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(next_->sequence_checker_);
      DCHECK_EQ(next_->prev_, this);
      next_->prev_ = prev_;
    }

    delete this;
  }

 private:
  // TODO(crbug.com/40117344): Extract the logic to manage a linked list
  // of WebContentsObservers to a helper class.
  const raw_ptr<PageLoadTrackerDecoratorHelper> outer_;
  raw_ptr<WebContentsObserver> prev_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<WebContentsObserver> next_ GUARDED_BY_CONTEXT(sequence_checker_);

  enum class LoadingState {
    // Initial state.
    // DidStartLoading():     Transition to kWaitingForNavigation.
    // PrimaryPageChanged():  Invalid from this state.
    // DidStopLoading():      Invalid from this state.
    kNotLoading,
    // DidStartLoading():     Invalid from this state.
    // PrimaryPageChanged():  Transition to kLoading.
    // DidStopLoading():      Transition to kNotLoading.
    kWaitingForNavigation,
    // DidStartLoading():     Invalid from this state.
    // PrimaryPageChanged():  Invalid from this state.
    // DidStopLoading():      Transition to kNotLoading.
    kLoading,
  };

  LoadingState loading_state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      LoadingState::kNotLoading;

  SEQUENCE_CHECKER(sequence_checker_);
};

PageLoadTrackerDecoratorHelper::PageLoadTrackerDecoratorHelper() {
  PerformanceManager::AddObserver(this);
}

PageLoadTrackerDecoratorHelper::~PageLoadTrackerDecoratorHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Destroy all WebContentsObserver to ensure that PageLiveStateDecorators are
  // no longer maintained.
  while (first_web_contents_observer_)
    first_web_contents_observer_->DetachAndDestroy();

  PerformanceManager::RemoveObserver(this);
}

void PageLoadTrackerDecoratorHelper::OnPageNodeCreatedForWebContents(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);
  // Start observing the WebContents. See comment on
  // |first_web_contents_observer_| for lifetime management details.
  new WebContentsObserver(web_contents, this);
}

}  // namespace performance_manager
