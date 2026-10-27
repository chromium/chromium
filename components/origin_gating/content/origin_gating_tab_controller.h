// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CONTENT_ORIGIN_GATING_TAB_CONTROLLER_H_
#define COMPONENTS_ORIGIN_GATING_CONTENT_ORIGIN_GATING_TAB_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/origin_gating/core/checker_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/mojom/window_open_disposition.mojom-forward.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace content {
class RenderFrameHost;
struct Referrer;
class WebContents;
}  // namespace content

namespace origin_gating {

class OriginGatingChecker;
class OriginGatingService;

// Associates a `content::WebContents` with an active `OriginGatingChecker`
// registered in `OriginGatingService`, and automatically propagates that
// association to child `WebContents` (e.g. popups/new tabs) and across tab
// discards.
class OriginGatingTabController
    : public content::WebContentsUserData<OriginGatingTabController>,
      public content::WebContentsObserver {
 public:
  ~OriginGatingTabController() override;

  OriginGatingTabController(const OriginGatingTabController&) = delete;
  OriginGatingTabController& operator=(const OriginGatingTabController&) =
      delete;

  // Associates this tab with `checker_id` if this tab is not already
  // associated with a live `OriginGatingChecker` and `checker_id` refers to a
  // live `OriginGatingChecker` in `OriginGatingService`. `checker_id` must be
  // non-null; to clear the association, call `ClearCheckerId()`.
  void SetCheckerId(CheckerId checker_id);

  // Clears the `CheckerId` associated with this tab (e.g. when a task pauses
  // or finishes controlling the tab).
  void ClearCheckerId();

  // Returns the active `OriginGatingChecker` for this tab, or `nullptr` if
  // `checker_id_` is unset or is no longer registered in `OriginGatingService`.
  OriginGatingChecker* GetChecker();

 private:
  friend class content::WebContentsUserData<OriginGatingTabController>;

  OriginGatingTabController(content::WebContents* web_contents,
                            OriginGatingService& service);

  CheckerId checker_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return checker_id_;
  }

  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           ui::mojom::WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void AboutToBeDiscarded(content::WebContents* new_contents) override;

  void PropagateToNewContents(content::WebContents* new_contents);

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ref<OriginGatingService> service_
      GUARDED_BY_CONTEXT(sequence_checker_);
  CheckerId checker_id_ GUARDED_BY_CONTEXT(sequence_checker_);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CONTENT_ORIGIN_GATING_TAB_CONTROLLER_H_
