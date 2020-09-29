// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRERENDER_BROWSER_PRERENDER_CONTENTS_H_
#define COMPONENTS_PRERENDER_BROWSER_PRERENDER_CONTENTS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prerender/browser/prerender_contents_delegate.h"
#include "components/prerender/common/prerender_canceler.mojom.h"
#include "components/prerender/common/prerender_final_status.h"
#include "components/prerender/common/prerender_origin.h"
#include "components/prerender/common/prerender_types.mojom.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
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
}  // namespace content

namespace memory_instrumentation {
class GlobalMemoryDump;
}

namespace prerender {

class PrerenderManager;

class PrerenderContents : public content::NotificationObserver,
                          public content::WebContentsObserver,
                          public prerender::mojom::PrerenderCanceler {
 public:
  // PrerenderContents::Create uses the currently registered Factory to create
  // the PrerenderContents. Factory is intended for testing.
  class Factory {
   public:
    Factory() {}
    virtual ~Factory() {}

    // Ownership is not transferred through this interface as prerender_manager
    // and browser_context are stored as weak pointers.
    virtual PrerenderContents* CreatePrerenderContents(
        std::unique_ptr<PrerenderContentsDelegate> delegate,
        PrerenderManager* prerender_manager,
        content::BrowserContext* browser_context,
        const GURL& url,
        const content::Referrer& referrer,
        const base::Optional<url::Origin>& initiator_origin,
        Origin origin) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  class Observer {
   public:
    // Signals that the prerender has started running.
    virtual void OnPrerenderStart(PrerenderContents* contents) {}

    // Signals that the prerender has had its load event.
    virtual void OnPrerenderStopLoading(PrerenderContents* contents) {}

    // Signals that the prerender has had its 'DOMContentLoaded' event.
    virtual void OnPrerenderDomContentLoaded(PrerenderContents* contents) {}

    // Signals that the prerender has stopped running. A PrerenderContents with
    // an unset final status will always call OnPrerenderStop before being
    // destroyed.
    virtual void OnPrerenderStop(PrerenderContents* contents) {}

    // Signals that a resource finished loading and altered the running byte
    // count.
    virtual void OnPrerenderNetworkBytesChanged(PrerenderContents* contents) {}

   protected:
    Observer() {}
    virtual ~Observer() = 0;
  };

  ~PrerenderContents() override;

  // All observers of a PrerenderContents are removed after the OnPrerenderStop
  // event is sent, so there is no need to call RemoveObserver() in the normal
  // use case.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool Init();

  // Set the mode of this contents. This must be called before prerender has
  // started.
  void SetPrerenderMode(prerender::mojom::PrerenderMode mode);
  prerender::mojom::PrerenderMode prerender_mode() const {
    return prerender_mode_;
  }

  static Factory* CreateFactory();

  // Starts rendering the contents in the prerendered state.
  // |bounds| indicates the rectangle that the prerendered page should be in.
  // |session_storage_namespace| indicates the namespace that the prerendered
  // page should be part of.
  virtual void StartPrerendering(
      const gfx::Rect& bounds,
      content::SessionStorageNamespace* session_storage_namespace);

  // Verifies that the prerendering is not using too many resources, and kills
  // it if not.
  void DestroyWhenUsingTooManyResources();

  content::RenderViewHost* GetRenderViewHost();

  PrerenderManager* prerender_manager() { return prerender_manager_; }

  const GURL& prerender_url() const { return prerender_url_; }
  bool has_finished_loading() const { return has_finished_loading_; }
  bool prerendering_has_started() const { return prerendering_has_started_; }

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
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void RenderProcessGone(base::TerminationStatus status) override;

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Checks that a URL may be prerendered, for one of the many redirections. If
  // the URL can not be prerendered - for example, it's an ftp URL - |this| will
  // be destroyed and false is returned. Otherwise, true is returned.
  virtual bool CheckURL(const GURL& url);

  // Adds an alias URL. If the URL can not be prerendered, |this| will be
  // destroyed and false is returned.
  bool AddAliasURL(const GURL& url);

  // The prerender WebContents (may be NULL).
  content::WebContents* prerender_contents() const {
    return prerender_contents_.get();
  }

  std::unique_ptr<content::WebContents> ReleasePrerenderContents();

  // Sets the final status, calls OnDestroy and adds |this| to the
  // PrerenderManager's pending deletes list.
  void Destroy(FinalStatus reason);

  std::unique_ptr<base::DictionaryValue> GetAsValue() const;

  // This function is not currently called in production since prerendered
  // contents are never used (only prefetch is supported), but it may be used in
  // the future: https://crbug.com/1126305
  void MarkAsUsedForTesting();

  // Increments the number of bytes fetched over the network for this prerender.
  void AddNetworkBytes(int64_t bytes);

  bool prerendering_has_been_cancelled() const {
    return prerendering_has_been_cancelled_;
  }

  // Running byte count. Increased when each resource completes loading.
  int64_t network_bytes() { return network_bytes_; }

  void AddPrerenderCancelerReceiver(
      mojo::PendingReceiver<prerender::mojom::PrerenderCanceler> receiver);

 protected:
  PrerenderContents(std::unique_ptr<PrerenderContentsDelegate> delegate,
                    PrerenderManager* prerender_manager,
                    content::BrowserContext* browser_context,
                    const GURL& url,
                    const content::Referrer& referrer,
                    const base::Optional<url::Origin>& initiator_origin,
                    Origin origin);

  // Set the final status for how the PrerenderContents was used. This
  // should only be called once, and should be called before the prerender
  // contents are destroyed.
  void SetFinalStatus(FinalStatus final_status);

  // These call out to methods on our Observers, using our observer_list_. Note
  // that NotifyPrerenderStop() also clears the observer list.
  void NotifyPrerenderStart();
  void NotifyPrerenderStopLoading();
  void NotifyPrerenderDomContentLoaded();
  void NotifyPrerenderStop();

  // Called whenever a RenderViewHost is created for prerendering.  Only called
  // once the RenderViewHost has a RenderView and RenderWidgetHostView.
  virtual void OnRenderViewHostCreated(
      content::RenderViewHost* new_render_view_host);

  std::unique_ptr<content::WebContents> CreateWebContents(
      content::SessionStorageNamespace* session_storage_namespace);

  prerender::mojom::PrerenderMode prerender_mode_;
  bool prerendering_has_started_;

  // Time at which we started to load the URL.  This is used to compute
  // the time elapsed from initiating a prerender until the time the
  // (potentially only partially) prerendered page is shown to the user.
  base::TimeTicks load_start_time_;

  // The prerendered WebContents; may be null.
  std::unique_ptr<content::WebContents> prerender_contents_;

  // The session storage namespace id for use in matching. We must save it
  // rather than get it from the RenderViewHost since in the control group
  // we won't have a RenderViewHost.
  std::string session_storage_namespace_id_;

 private:
  class WebContentsDelegateImpl;

  // Needs to be able to call the constructor.
  friend class PrerenderContentsFactoryImpl;

  // Returns the ProcessMetrics for the render process, if it exists.
  void DidGetMemoryUsage(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  // prerender::mojom::PrerenderCanceler:
  void CancelPrerenderForUnsupportedScheme() override;
  void CancelPrerenderForNoStatePrefetch() override;

  mojo::ReceiverSet<prerender::mojom::PrerenderCanceler>
      prerender_canceler_receiver_set_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  // The prerender manager owning this object.
  PrerenderManager* prerender_manager_;

  // The delegate that content embedders use to override this class's logic.
  std::unique_ptr<PrerenderContentsDelegate> delegate_;

  // The URL being prerendered.
  const GURL prerender_url_;

  // The referrer.
  const content::Referrer referrer_;

  // The origin of the page requesting the prerender. Empty when the prerender
  // is browser initiated.
  const base::Optional<url::Origin> initiator_origin_;

  // The browser context being used
  content::BrowserContext* browser_context_;

  content::NotificationRegistrar notification_registrar_;

  // A vector of URLs that this prerendered page matches against.
  // This array can contain more than element as a result of redirects,
  // such as HTTP redirects or javascript redirects.
  std::vector<GURL> alias_urls_;

  // True when the main frame has finished loading.
  bool has_finished_loading_;

  FinalStatus final_status_;

  // Tracks whether or not prerendering has been cancelled by calling Destroy.
  // Used solely to prevent double deletion.
  bool prerendering_has_been_cancelled_;

  // Pid of the render process associated with the RenderViewHost for this
  // object.
  base::ProcessId process_pid_;

  std::unique_ptr<WebContentsDelegateImpl> web_contents_delegate_;

  // Origin for this prerender.
  const Origin origin_;

  // The bounds of the WebView from the launching page.
  gfx::Rect bounds_;

  // A running tally of the number of bytes this prerender has caused to be
  // transferred over the network for resources.  Updated with AddNetworkBytes.
  int64_t network_bytes_;

  base::WeakPtrFactory<PrerenderContents> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrerenderContents);
};

}  // namespace prerender

#endif  // COMPONENTS_PRERENDER_BROWSER_PRERENDER_CONTENTS_H_
