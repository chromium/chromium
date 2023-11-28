// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_CUSTOM_CURSOR_SUPPRESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_CUSTOM_CURSOR_SUPPRESSOR_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_host_registry.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {
class ExtensionHost;
}  // namespace extensions

// While active, this class suppresses custom cursors exceeding a given size
// limit on all the active `WebContents` or all `Browser`s of the current
// profile.
// Note that `CustomCursorSuppressor` is expected to have a short lifetime, e.g.
// while an Autofill popup is showing - therefore it does not clean up (safe,
// but stale) observation entries related to `RenderFrameHost`s of previous
// navigations or of `WebContents` that are no longer active.
// Should the class become used in a wider context, additional logic to remove
// such stale entries should be added.
class CustomCursorSuppressor
    : public BrowserListObserver,
      public TabStripModelObserver,
      public extensions::ExtensionHostRegistry::Observer {
 public:
  CustomCursorSuppressor();
  CustomCursorSuppressor(const CustomCursorSuppressor&) = delete;
  CustomCursorSuppressor(CustomCursorSuppressor&&) = delete;
  CustomCursorSuppressor& operator=(const CustomCursorSuppressor&) = delete;
  CustomCursorSuppressor& operator=(CustomCursorSuppressor&&) = delete;
  ~CustomCursorSuppressor() override;

  // Starts suppressing cursors with height or width >= `max_dimension_dips` on
  // all active tabs of all browser windows.
  void Start(int max_dimension_dips = 0);
  // Stops suppressing custom cursors.
  void Stop();

  // Returns whether custom cursors are disallowed on `web_contents`.
  bool IsSuppressing(content::WebContents& web_contents) const;

  // Returns the ids of `RenderFrameHost`s on which custom cursors are
  // suppressed. Note that not every id needs to correspond to an active
  // `RenderFrameHost` - some may already have been deleted.
  std::vector<content::GlobalRenderFrameHostId>
  SuppressedRenderFrameHostIdsForTesting() const;

 private:
  // Starts observing the `ExtensionHostRegistry` for profile and suppresses
  // all custom cursors in its extensions. This is a no-op if the profile
  // custom cursors for the extensions of this profile are already suppressed.
  void ObserveAndSuppressExtensionsForProfile(Profile& profile);

  // Disallows custom cursors beyond the permitted size on `web_contents`. If
  // `this` is already disallowing custom cursors on `web_contents`, this is a
  // no-op.
  void SuppressForWebContents(content::WebContents& web_contents);

  // Observes navigations in `web_contents` that lead to changes in the
  // `RenderFrameHost` of the primary main frame iff custom cursors are not
  // already disallowed on `web_contents`.
  void MaybeObserveNavigationsInWebContents(content::WebContents& web_contents);

  // BrowserListObserver:
  // Starts observing the tab strip model of `browser`. Note that there is
  // no corresponding `OnBrowserRemoved`, since `TabStripModelObserver`
  // already handles model destruction itself.
  void OnBrowserAdded(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // extensions::ExtensionHostRegistry::Observer:
  void OnExtensionHostDocumentElementAvailable(
      content::BrowserContext* browser_context,
      extensions::ExtensionHost* extension_host) override;
  void OnExtensionHostRegistryShutdown(
      extensions::ExtensionHostRegistry* registry) override;

  // A helper to filter and forward `RenderFrameHostChanged` events of a single
  // `WebContents`. Used to allow `CustomCursorSuppressor` to effectively
  // observe multiple `WebContents`.
  class NavigationObserver : public content::WebContentsObserver {
   public:
    // A callback that is called whenever the `RenderFrameHost` of the primary
    // main frame of the observed `WebContents` has changed.
    using Callback = base::RepeatingCallback<void(content::WebContents&)>;

    NavigationObserver(content::WebContents* web_contents, Callback callback);
    ~NavigationObserver() override;

   private:
    // content::WebContentsObserver:
    void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                                content::RenderFrameHost* new_host) override;

    Callback callback_;
  };

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};

  // Observes when new `ExtensionHost`s load their documents and when
  // `ExtensionHostRegistry`s shut down.
  base::ScopedMultiSourceObservation<
      extensions::ExtensionHostRegistry,
      extensions::ExtensionHostRegistry::Observer>
      extension_host_registry_observation_{this};

  int max_dimension_dips_ = 0;

  std::map<content::GlobalRenderFrameHostId, base::ScopedClosureRunner>
      disallow_custom_cursor_scopes_;

  std::vector<std::unique_ptr<NavigationObserver>> navigation_observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_CUSTOM_CURSOR_SUPPRESSOR_H_
