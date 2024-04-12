// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {

// A feature which wants to show tab-modal UI should call
// TabInterface::ShowModalUI and keep alive the instance of ScopedTabModalUI for
// the duration of the tab-modal UI.
class ScopedTabModalUI {
 public:
  ScopedTabModalUI() = default;
  virtual ~ScopedTabModalUI() = default;
};

// This is the public interface for tabs in a desktop browser. Most features in
// //chrome/browser depend on this interface, and thus to prevent circular
// dependencies this interface should not depend on anything else in //chrome.
class TabInterface {
 public:
  // When a tab is in the background, the WebContents may be discarded to save
  // memory. When a tab is in the foreground it is guaranteed to have a
  // WebContents.
  virtual content::WebContents* GetContents() const = 0;

  // Register for these two callbacks to detect changes to GetContents().
  using DidAddContentsCallback =
      base::RepeatingCallback<void(TabInterface*, content::WebContents*)>;
  virtual base::CallbackListSubscription RegisterDidAddContents(
      DidAddContentsCallback callback) = 0;

  using WillRemoveContentsCallback =
      base::RepeatingCallback<void(TabInterface*, content::WebContents*)>;
  virtual base::CallbackListSubscription RegisterWillRemoveContents(
      WillRemoveContentsCallback callback) = 0;

  // Whether the tab is in the foreground. When a tab is in the foreground, this
  // class guarantees that GetContents() will return a non-nullptr WebContents,
  // and this WebContents will not change.
  virtual bool IsInForeground() const = 0;

  // Register for these two callbacks to detect changes to IsForegrounded().
  // DidEnterBackgroundCallback can be called repeatedly while the Tab remains
  // in the background.
  using DidEnterForegroundCallback =
      base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterDidEnterForeground(
      DidEnterForegroundCallback callback) = 0;

  using DidEnterBackgroundCallback =
      base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterDidEnterBackground(
      DidEnterBackgroundCallback callback) = 0;

  // Features that want to show tab-modal UI are mutually exclusive. Before
  // showing a modal UI first check `CanShowModal`. Then call ShowModal() and
  // keep `ScopedTabModal` alive to prevent other features from showing
  // tab-modal UI.
  virtual bool CanShowModalUI() const = 0;
  virtual std::unique_ptr<ScopedTabModalUI> ShowModalUI() = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_
