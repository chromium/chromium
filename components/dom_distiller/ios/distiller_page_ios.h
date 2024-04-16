// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_IOS_H_
#define COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_IOS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "ios/web/public/web_state_observer.h"
#include "url/gurl.h"

namespace web {
class BrowserState;
class WebState;
}

namespace dom_distiller {

class DistillerPageMediaBlocker;

// Loads URLs and injects JavaScript into a page, extracting the distilled page
// content.
class DistillerPageIOS : public DistillerPage, public web::WebStateObserver {
 public:
  explicit DistillerPageIOS(web::BrowserState* browser_state);

  DistillerPageIOS(const DistillerPageIOS&) = delete;
  DistillerPageIOS& operator=(const DistillerPageIOS&) = delete;

  ~DistillerPageIOS() override;

 protected:
  bool StringifyOutput() override;
  void DistillPageImpl(const GURL& url, const std::string& script) override;

  // Sets the WebState that will be used for the distillation. Do not call
  // between |DistillPageImpl| and |OnDistillationDone|.
  virtual void AttachWebState(std::unique_ptr<web::WebState> web_state);

  // Release the WebState used for distillation. Do not call between
  // |DistillPageImpl| and |OnDistillationDone|.
  virtual std::unique_ptr<web::WebState> DetachWebState();

  // Return the current WebState.
  virtual web::WebState* CurrentWebState();

  // Called by |web_state_observer_| once the page has finished loading.
  virtual void OnLoadURLDone(
      web::PageLoadCompletionStatus load_completion_status);

 private:
  // Called once the |script_| has been evaluated on the page.
  void HandleJavaScriptResult(const base::Value* result);

  // web::WebStateObserver implementation.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidStartLoading(web::WebState* web_state) override;
  void DidStopLoading(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  GURL url_;
  std::string script_;
  raw_ptr<web::BrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<DistillerPageMediaBlocker> media_blocker_;
  bool distilling_navigation_ = false;

  // Used to store whether the owned WebState is currently loading or not.
  // TODO(crbug.com/40548473): this is a work-around as WebState::IsLoading()
  // is/was not returning the expected value when an SLL interstitial is
  // blocked. Remove this and use WebState::IsLoading() when WebState has
  // been fixed.
  bool loading_ = false;

  base::WeakPtrFactory<DistillerPageIOS> weak_ptr_factory_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_IOS_H_
