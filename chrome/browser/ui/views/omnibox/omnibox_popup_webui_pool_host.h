// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_HOST_H_

#include <memory>
#include <string_view>

#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"

namespace views {
class Widget;
}  // namespace views

class OmniboxPopupUI;
class OmniboxPopupWebUIBaseContent;
template <typename T>
class WebUIContentsWrapperT;

// What `OmniboxPopupWebUIPool` and `OmniboxPopupWebUIPoolClient` need from the
// object that hosts a WebUI omnibox popup in a browser window, so that the
// windows of a profile can share the popup. Implemented by the popup
// presenters. A host destroys its `OmniboxPopupWebUIPoolClient` before it
// clears its content, so that the pool can tell whether the host closed with
// the last popup of its kind.
class OmniboxPopupWebUIPoolHost {
 public:
  virtual ~OmniboxPopupWebUIPoolHost() = default;

  // Returns the kind of popup that this host shares with the profile's other
  // browser windows, or `OmniboxPopupState::kNone` if it keeps its own.
  virtual OmniboxPopupState GetSharedPopupKind() const = 0;

  // Returns the WebUI content view, or null if the host has none.
  virtual OmniboxPopupWebUIBaseContent* GetWebUIContent() = 0;

  // Returns whether `ReleaseWebUIContents()` may move the popup to another
  // browser window now.
  virtual bool CanReleaseWebUIContents() = 0;

  // Releases the popup's WebContents, so that the host in another browser
  // window of the profile can adopt it with `AdoptWebUIContents()`.
  virtual std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
  ReleaseWebUIContents() = 0;

  // Creates this host's content around `contents`, which another browser
  // window's host released, instead of loading a new WebContents.
  virtual OmniboxPopupWebUIBaseContent* AdoptWebUIContents(
      std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents) = 0;

  // Returns the browser window that hosts the popup, or null before the host
  // is in one.
  virtual views::Widget* GetBrowserWidget() const = 0;

  // Returns the prefix of the popup's histograms.
  virtual std::string_view GetPopupMetricPrefix() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_HOST_H_
