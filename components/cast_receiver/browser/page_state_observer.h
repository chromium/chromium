// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PAGE_STATE_OBSERVER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PAGE_STATE_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace cast_receiver {

// Wrapper around a WebContentsObserver to expose simple lifetime events for
// a WebContents.
class PageStateObserver {
 public:
  // Reason which a page load may have stopped. Used when calling
  // OnPageStopped() below.
  enum class StopReason { kUnknown = 0, kApplicationRequest, kHttpError };

  virtual ~PageStateObserver();

  // Called when the observed |web_contents| has completed loading of a page,
  // either by completing loading of the page's contents or by user action to
  // cancel the navigation.
  virtual void OnPageLoadComplete() {}

  // Called when the observed |web_contents| stops trying to load the page for
  // reasons outside of the user's control - such as the page closing or an
  // HTTP error.
  virtual void OnPageStopped(StopReason reason, net::Error error_code) {}

 protected:
  PageStateObserver();
  explicit PageStateObserver(content::WebContents* web_contents);

  void Observe(content::WebContents* web_contents);

 private:
  // Implementation of the WebContentsObserver interface to call into the
  // functions defined above. This extra later of indirection is used rather
  // than directly observing the |web_contents| to avoid complexity associated
  // with implementers of this class which wish to also implement
  // content::WebContentsObserver.
  class WebContentsObserverWrapper;

  std::unique_ptr<WebContentsObserverWrapper> observer_wrapper_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PAGE_STATE_OBSERVER_H_
