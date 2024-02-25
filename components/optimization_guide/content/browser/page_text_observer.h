// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_OBSERVER_H_

#include <stdint.h>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/content/browser/page_text_dump_result.h"
#include "components/optimization_guide/content/mojom/page_text_service.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace optimization_guide {

// Provides callers with the text from web pages that they choose to request.
// Currently, the only method of obtaining text from the page is by recursively
// iterating through all DOM nodes. This is a very expensive operation which
// should be avoided whenever possible. Features that wish to get page text need
// to implement the |Consumer| interface. |Consumer::MaybeRequestFrameTextDump|
// is called on every navigation commit, at which time consumers must decide
// whether to request the page text to be dumped, and at what renderer event.
// This service will de-duplicate the requests and serve the responses to the
// provided callback.
class PageTextObserver : public content::WebContentsObserver,
                         public content::WebContentsUserData<PageTextObserver> {
 public:
  ~PageTextObserver() override;

  // Retrieves the instance of PageTextObserver that was attached
  // to the specified WebContents. If no instance was attached, creates one,
  // and attaches it to the specified WebContents.
  static PageTextObserver* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Contains all the information that is needed to request a text dump by a
  // consumer.
  struct ConsumerTextDumpRequest {
   public:
    ConsumerTextDumpRequest();
    ~ConsumerTextDumpRequest();

    // The callback that is used to provide dumped page text.
    using TextDumpCallback =
        base::OnceCallback<void(const PageTextDumpResult&)>;
    TextDumpCallback callback;

    // The max size of the text dump in bytes. Note that the actual size
    // that is passed in the callback may actually be greater than this value if
    // another consumer requests a greater amount on the same event, or less on
    // pages with little text.
    uint32_t max_size = 0;

    // Set when subframe text dumps should be taken on AMP subframes. A text
    // dump of the mainframe will always also be taken. Consumer who set this
    // should use |PageTextDumpResult::ConcatenateWithAMPHandling| on the
    // |callback|.
    bool dump_amp_subframes = false;

    // All of the |TextDumpEvent|'s that have been requested.
    std::set<mojom::TextDumpEvent> events;
  };

  // Callers should implement this class to request text dumps of pages at
  // commit time.
  class Consumer {
   public:
    // Called at commit of every main frame navigation. Consumers should return
    // a request if they want to get the page text, or nullptr if not.
    virtual std::unique_ptr<ConsumerTextDumpRequest> MaybeRequestFrameTextDump(
        content::NavigationHandle* handle) = 0;
  };

  // Adds or removes a consumer. Consumers must remain valid between calling Add
  // and Remove. Virtual for testing.
  virtual void AddConsumer(Consumer* consumer);
  virtual void RemoveConsumer(Consumer* consumer);

  size_t outstanding_requests() const { return outstanding_requests_; }

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void RenderFrameCreated(content::RenderFrameHost* rfh) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  PageTextObserver(const PageTextObserver&) = delete;
  PageTextObserver& operator=(const PageTextObserver&) = delete;

 protected:
  explicit PageTextObserver(content::WebContents* web_contents);

  // Virtual for testing.
  virtual bool IsOOPIF(content::RenderFrameHost* rfh) const;

 private:
  friend class content::WebContentsUserData<PageTextObserver>;

  void OnFrameTextDumpCompleted(
      std::optional<FrameTextDumpResult> frame_result);

  void DispatchResponses();

  // All registered consumers.
  std::set<raw_ptr<Consumer, SetExperimental>> consumers_;

  // A persisted set of consumer requests.
  std::vector<std::unique_ptr<ConsumerTextDumpRequest>> requests_;

  std::unique_ptr<PageTextDumpResult> page_result_;

  // |outstanding_requests_grace_timer_| is set after |DidFinishLoad| if the
  // number of |outstanding_requests_| is > 0. When the timer fires, the
  // |page_result_| will be finialized and dispatched to consumers (in
  // |DispatchResponses|).
  std::unique_ptr<base::OneShotTimer> outstanding_requests_grace_timer_;
  size_t outstanding_requests_ = 0;

  base::WeakPtrFactory<PageTextObserver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_OBSERVER_H_
