// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_OBSERVER_H_

#include <stdint.h>
#include <set>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
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
        base::RepeatingCallback<void(const base::string16&)>;
    TextDumpCallback callback;

    // The max size of the text dump in bytes. Note that the actual size
    // that is passed in the callback may actually be greater than this value if
    // another consumer requests a greater amount on the same event, or less on
    // pages with little text.
    uint32_t max_size = 0;

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

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  PageTextObserver(const PageTextObserver&) = delete;
  PageTextObserver& operator=(const PageTextObserver&) = delete;

 protected:
  explicit PageTextObserver(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<PageTextObserver>;

  // All registered consumers.
  std::set<Consumer*> consumers_;

  base::WeakPtrFactory<PageTextObserver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_OBSERVER_H_
