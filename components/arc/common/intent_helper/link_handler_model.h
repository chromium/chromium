// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_INTENT_HELPER_LINK_HANDLER_MODEL_H_
#define COMPONENTS_ARC_COMMON_INTENT_HELPER_LINK_HANDLER_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/arc/common/intent_helper/arc_icon_cache_delegate.h"
#include "components/arc/common/intent_helper/arc_intent_helper_mojo_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

// This struct describes the UI presentation of a single link handler.
struct LinkHandlerInfo {
  std::u16string name;
  gfx::Image icon;
  // An opaque identifier for this handler (which happens to correlate to the
  // index in |handlers_|.
  uint32_t id;
};

class LinkHandlerModel {
 public:
  class Observer {
   public:
    virtual void ModelChanged(const std::vector<LinkHandlerInfo>& handlers) = 0;
  };

  // Creates and inits a model. Will return null if Init() fails.
  static std::unique_ptr<LinkHandlerModel> Create(
      content::BrowserContext* context,
      const GURL& link_url,
      std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate);

  LinkHandlerModel(const LinkHandlerModel&) = delete;
  LinkHandlerModel& operator=(const LinkHandlerModel&) = delete;
  ~LinkHandlerModel();

  void AddObserver(Observer* observer);

  void OpenLinkWithHandler(uint32_t handler_id);

  static GURL RewriteUrlFromQueryIfAvailableForTesting(const GURL& url);

 private:
  explicit LinkHandlerModel(
      std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate);

  // Starts retrieving handler information for the |url| and returns true.
  // Returns false when the information cannot be retrieved. In that case,
  // the caller should delete |this| object.
  bool Init(content::BrowserContext* context, const GURL& url);

  void OnUrlHandlerList(
      std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers);
  void NotifyObserver(
      std::unique_ptr<ArcIconCacheDelegate::ActivityToIconsMap> icons);

  // Checks if the |url| matches the following pattern:
  //   "http(s)://<valid_google_hostname>/url?...&url=<valid_url>&..."
  // If it does, creates a new GURL object from the <valid_url> and returns it.
  // Otherwise, returns the original |url| as-us.
  static GURL RewriteUrlFromQueryIfAvailable(const GURL& url);

  raw_ptr<content::BrowserContext> context_ = nullptr;

  GURL url_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  // Url handler info passed from ARC.
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers_;
  // Activity icon info passed from ARC.
  ArcIconCacheDelegate::ActivityToIconsMap icons_;

  // a delegate instance for calling mojo API.
  std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate_;

  base::WeakPtrFactory<LinkHandlerModel> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_INTENT_HELPER_LINK_HANDLER_MODEL_H_
