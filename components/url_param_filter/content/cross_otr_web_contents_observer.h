// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PARAM_FILTER_CONTENT_CROSS_OTR_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_URL_PARAM_FILTER_CONTENT_CROSS_OTR_WEB_CONTENTS_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "components/url_param_filter/core/cross_otr_observer.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/page_transition_types.h"

namespace url_param_filter {

// This class utilizes the state-machine logic driven by the CrossOtrObserver,
// which handles all logging of metrics and informs this class when it should be
// detached upon existing "CrossOTR" state.
class CrossOtrWebContentsObserver
    : public CrossOtrObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<CrossOtrWebContentsObserver> {
 public:
  ~CrossOtrWebContentsObserver() override;

  // Attaches the observer in cases where it should do so; leaves `web_contents`
  // unchanged otherwise.
  static void MaybeCreateForWebContents(content::WebContents* web_contents,
                                        bool is_cross_otr,
                                        bool started_from_context_menu,
                                        ui::PageTransition transition);
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;

  // Inherited from content::WebContentsUserData, but should not be used outside
  // this class or its Android counterpart. MaybeCreateForWebContents should be
  // used instead.
  using content::WebContentsUserData<
      CrossOtrWebContentsObserver>::CreateForWebContents;

  base::WeakPtr<CrossOtrWebContentsObserver> GetWeakPtr();

 private:
  explicit CrossOtrWebContentsObserver(content::WebContents* web_contents);

  friend class content::WebContentsUserData<CrossOtrWebContentsObserver>;
  // Flushes metrics and removes the observer from the WebContents.
  void Detach();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  base::WeakPtrFactory<CrossOtrWebContentsObserver> weak_factory_;
};

}  // namespace url_param_filter
#endif  // COMPONENTS_URL_PARAM_FILTER_CONTENT_CROSS_OTR_WEB_CONTENTS_OBSERVER_H_
