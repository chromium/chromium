// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_TAB_FEATURE_H_
#define CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_TAB_FEATURE_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/tracked_element_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

namespace content {
class RenderWidgetHost;
class WebContents;
}  // namespace content

class ContextHighlightWindowFeature;

namespace tabs {

// This class is responsible for tracking AI-generated highlight bounds for a
// specific tab. it observes the tab's WebContents and RenderWidgetHost to
// receive updates about tracked element bounds.
class ContextHighlightTabFeature : public content::TrackedElementObserver,
                                   public content::WebContentsObserver {
 public:
  explicit ContextHighlightTabFeature(TabInterface& tab);
  ~ContextHighlightTabFeature() override;

  ContextHighlightTabFeature(const ContextHighlightTabFeature&) = delete;
  ContextHighlightTabFeature& operator=(const ContextHighlightTabFeature&) =
      delete;

  static ContextHighlightTabFeature* From(TabInterface* tab);

  // Returns the current RenderWidgetHost for the tab's WebContents.
  content::RenderWidgetHost* GetRenderWidgetHost() const;

  const cc::TrackedElementBounds& latest_bounds() const {
    return latest_bounds_;
  }

  float latest_scale_factor() const { return latest_scale_factor_; }

  // content::TrackedElementObserver:
  void OnTrackedElementBoundsChanged(const cc::TrackedElementBounds& bounds,
                                     float device_scale_factor) override;

  // content::WebContentsObserver:
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  DECLARE_USER_DATA(ContextHighlightTabFeature);

 private:
  // Called when the RenderWidgetHost for the tab changes.
  void RenderWidgetHostChanged(content::RenderWidgetHost* old_host,
                               content::RenderWidgetHost* new_host);

  // Called when the tab's WebContents are about to be discarded.
  void OnWillDiscardContents(TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents);

  // Registers this object as an observer of the given RenderWidgetHost.
  void RegisterObserverWithHost(content::RenderWidgetHost* host);

  // Unregisters this object as an observer of the given RenderWidgetHost.
  void UnregisterObserverFromHost(content::RenderWidgetHost* host);

  // Returns the window-scoped feature associated with this tab.
  ContextHighlightWindowFeature* GetWindowFeature();

  raw_ptr<TabInterface> tab_;
  raw_ptr<content::RenderWidgetHost> current_host_ = nullptr;
  base::CallbackListSubscription discard_subscription_;
  base::CallbackListSubscription tab_activate_subscription_;

  cc::TrackedElementBounds latest_bounds_;
  float latest_scale_factor_ = 1.0f;

  ui::ScopedUnownedUserData<ContextHighlightTabFeature> scoped_data_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_TAB_FEATURE_H_
