// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_H_
#define COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

// This struct describes the UI presentation of a single link handler.
struct LinkHandlerInfo {
  base::string16 name;
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
      const GURL& link_url);

  ~LinkHandlerModel();

  void AddObserver(Observer* observer);

  void OpenLinkWithHandler(uint32_t handler_id);

  static GURL RewriteUrlFromQueryIfAvailableForTesting(const GURL& url);

 private:
  LinkHandlerModel();

  // Starts retrieving handler information for the |url| and returns true.
  // Returns false when the information cannot be retrieved. In that case,
  // the caller should delete |this| object.
  bool Init(content::BrowserContext* context, const GURL& url);

  void OnUrlHandlerList(std::vector<mojom::IntentHandlerInfoPtr> handlers);
  void NotifyObserver(
      std::unique_ptr<ArcIntentHelperBridge::ActivityToIconsMap> icons);

  // Checks if the |url| matches the following pattern:
  //   "http(s)://<valid_google_hostname>/url?...&url=<valid_url>&..."
  // If it does, creates a new GURL object from the <valid_url> and returns it.
  // Otherwise, returns the original |url| as-us.
  static GURL RewriteUrlFromQueryIfAvailable(const GURL& url);

  content::BrowserContext* context_ = nullptr;

  GURL url_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  // Url handler info passed from ARC.
  std::vector<mojom::IntentHandlerInfoPtr> handlers_;
  // Activity icon info passed from ARC.
  ArcIntentHelperBridge::ActivityToIconsMap icons_;

  base::WeakPtrFactory<LinkHandlerModel> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LinkHandlerModel);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_H_
