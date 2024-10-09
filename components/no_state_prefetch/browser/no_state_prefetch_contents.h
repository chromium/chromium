// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_CONTENTS_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_CONTENTS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents_delegate.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"
#include "components/no_state_prefetch/common/prerender_canceler.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

namespace base {
class ProcessMetrics;
}

namespace content {
class BrowserContext;
class RenderViewHost;
class SessionStorageNamespace;
class WebContents;
class PreloadingAttempt;
}  // namespace content

namespace memory_instrumentation {
class GlobalMemoryDump;
}

namespace prerender {

class NoStatePrefetchManager;

class NoStatePrefetchContents : public content::WebContentsObserver,
                                public prerender::mojom::PrerenderCanceler {
 public:
  // NoStatePrefetchContents::Create uses the currently registered Factory to
  // create the NoStatePrefetchContents. Factory is intended for testing.
  class Factory {
   public:
    Factory() = default;

    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    virtual ~Factory() = default;

    // Ownership is not transferred through this interface as
    // no_state_prefetch_manager and browser_context are stored as weak
    // pointers.
    virtual NoStatePrefetchContents* CreateNoStatePrefetchContents(
        std::unique_ptr<NoStatePrefetchContentsDelegate> delegate,
        NoStatePrefetchManager* no_state_prefetch_manager,
        content::BrowserContext* browser_context,
        const GURL& url,
        const content::Referrer& referrer,
        const std::optional<url::Origin>& initiator_origin,
        Origin origin) = 0;
  };

  class Observer {
   public:
    // Signals that the prefetch has started running.
    virtual void OnPrefetchStart(NoStatePrefetchContents* contents) {}

    // Signals that the prefetch has had its load event.
    virtual void OnPrefetchStopLoading(NoStatePrefetchContents* contents) {}

    // Signals that the prefetch has stopped running.
    // A NoStatePrefetchContents with an unset final status will always call
    // OnPrefetchStop before being destroyed.
    virtual void OnPrefetchStop(NoStatePrefetchContents* contents) {}

   protected:
    Observer() = default;
    virtual ~Observer() = 0;
  };

  NoStatePrefetchContents(const NoStatePrefetchContents&) = delete;
  NoStatePrefetchContents& operator=(const NoStatePrefetchContents&) = delete;

  ~NoStatePrefetchContents() override;

  // All observers of a NoStatePrefetchContents are removed after the
  // OnPrefetchStop event is sent, so there is no need to call RemoveObserver()
  // in the normal use case.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool Init();

  static Factory* CreateFactory();

  // Starts rendering the contents in the prerendered state.
  // |bounds| indicates the rectangle that the prerendered page should be in.
  // |session_storage_namespace| indicates the namespace that the prerendered
  // page should be part of. |preloading_attempt| allows to log metrics for this
  // NoStatePrefetch attempt.
  virtual void StartPrerendering(
      const gfx::Rect& bounds,
      content::SessionStorageNamespace* session_storage_namespace,
      base::WeakPtr<content::PreloadingAttempt> preloading_attempt);

  // Verifies that the prerendering is not using too many resources, and kills
  // it if not.
  void DestroyWhenUsingTooManyResources();

  content::RenderFrameHost* GetPrimaryMainFrame();

  NoStatePrefetchManager* no_state_prefetch_manager() {
    return no_state_prefetch_manager_;
  }

  const GURL& prefetch_url() const { return prefetch_url_; }
  bool has_finished_loading() const { return has_finished_loading_; }
  bool prefetching_has_started() const { return prefetching_has_started_; }

  FinalStatus final_status() const { return final_status_; }

  Origin origin() const { return origin_; }

  base::TimeTicks load_start_time() const { return load_start_time_; }

  // Indicates whether this prerendered page can be used for the provided
  // |url| and |session_storage_namespace|.
  bool Matches(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace) const;

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void DidStopLoading() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // Checks that a URL may be prerendered, for one of the many redirections. If
  // the URL can not be prerendered - for example, it's an ftp URL - |this| will
  // be destroyed and false is returned. Otherwise, true is returned.
  virtual bool CheckURL(const GURL& url);

  // Adds an alias URL. If the URL can not be prerendered, |this| will be
  // destroyed and false is returned.
  bool AddAliasURL(const GURL& url);

  // The WebContents for NoStatePrefetch (may be NULL).
  content::WebContents* no_state_prefetch_contents() const {
    return no_state_prefetch_contents_.get();
  }

  // Sets the final status, calls OnDestroy and adds |this| to the
  // NoStatePrefetchManager's pending deletes list.
  void Destroy(FinalStatus reason);

  std::optional<base::Value::Dict> GetAsDict() const;

  // This function is not currently called in production since prerendered
  // contents are never used (only prefetch is supported), but it may be used in
  // the future: https://crbug.com/1126305
  void MarkAsUsedForTesting();

  bool prefetching_has_been_cancelled() const {
    return prefetching_has_been_cancelled_;
  }

  void AddPrerenderCancelerReceiver(
      mojo::PendingReceiver<prerender::mojom::PrerenderCanceler> receiver);

 protected:
  NoStatePrefetchContents(
      std::unique_ptr<NoStatePrefetchContentsDelegate> delegate,
      NoStatePrefetchManager* no_state_prefetch_manager,
      content::BrowserContext* browser_context,
      const GURL& url,
      const content::Referrer& referrer,
      const std::optional<url::Origin>& initiator_origin,
      Origin origin);

  // Set the final status for how the NoStatePrefetchContents was used. This
  // should only be called once, and should be called before the prerender
  // contents are destroyed.
  void SetFinalStatus(FinalStatus final_status);

  // These call out to methods on our Observers, using our observer_list_. Note
  // that NotifyPrefetchStop() also clears the observer list.
  void NotifyPrefetchStart();
  void NotifyPrefetchStopLoading();
  void NotifyPrefetchStop();

  std::unique_ptr<content::WebContents> CreateWebContents(
      content::SessionStorageNamespace* session_storage_namespace);

  bool prefetching_has_started_ = false;

  // Time at which we started to load the URL.  This is used to compute
  // the time elapsed from initiating a prerender until the time the
  // (potentially only partially) prerendered page is shown to the user.
  base::TimeTicks load_start_time_;

  // The WebContents for NoStatePrefetch; may be null.
  std::unique_ptr<content::WebContents> no_state_prefetch_contents_;

  // The session storage namespace id for use in matching. We must save it
  // rather than get it from the RenderViewHost since in the control group
  // we won't have a RenderViewHost.
  std::string session_storage_namespace_id_;

 private:
  class WebContentsDelegateImpl;

  // Needs to be able to call the constructor.
  friend class NoStatePrefetchContentsFactoryImpl;

  // Returns the ProcessMetrics for the render process, if it exists.
  void DidGetMemoryUsage(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  // Sets PreloadingFailureReason based on status corresponding to the
  // |attempt_|.
  void SetPreloadingFailureReason(FinalStatus status);

  // prerender::mojom::PrerenderCanceler:
  void CancelPrerenderForUnsupportedScheme() override;
  void CancelPrerenderForNoStatePrefetch() override;

  mojo::ReceiverSet<prerender::mojom::PrerenderCanceler>
      prerender_canceler_receiver_set_;

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observer_list_;

  // The prefetch manager owning this object.
  raw_ptr<NoStatePrefetchManager> no_state_prefetch_manager_;

  // The delegate that content embedders use to override this class's logic.
  std::unique_ptr<NoStatePrefetchContentsDelegate> delegate_;

  // The URL being prefetched.
  const GURL prefetch_url_;

  // Store the PreloadingAttempt for this NoStatePrefetch attempt. We store
  // WeakPtr as it is possible that the PreloadingAttempt is deleted before the
  // NoStatePrefetch is deleted.
  base::WeakPtr<content::PreloadingAttempt> attempt_ = nullptr;

  // The referrer.
  const content::Referrer referrer_;

  // The origin of the page requesting the prerender. Empty when the prerender
  // is browser initiated.
  const std::optional<url::Origin> initiator_origin_;

  // The browser context being used
  raw_ptr<content::BrowserContext> browser_context_;

  // A vector of URLs that this prerendered page matches against.
  // This array can contain more than element as a result of redirects,
  // such as HTTP redirects or javascript redirects.
  std::vector<GURL> alias_urls_;

  // True when the main frame has finished loading.
  bool has_finished_loading_ = false;

  FinalStatus final_status_;

  // Tracks whether or not prefetching has been cancelled by calling Destroy.
  // Used solely to prevent double deletion.
  bool prefetching_has_been_cancelled_ = false;

  // Pid of the render process associated with the RenderViewHost for this
  // object.
  base::ProcessId process_pid_;

  std::unique_ptr<WebContentsDelegateImpl> web_contents_delegate_;

  // Origin for this prerender.
  const Origin origin_;

  // The bounds of the WebView from the launching page.
  gfx::Rect bounds_;

  base::WeakPtrFactory<NoStatePrefetchContents> weak_factory_{this};
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_CONTENTS_H_
