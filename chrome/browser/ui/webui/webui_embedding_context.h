// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_EMBEDDING_CONTEXT_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_EMBEDDING_CONTEXT_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

class BrowserWindowInterface;

// These utilities help manage embedding context relevant to a `host_contents`
// for WebUI clients. `host_contents` is the WebContents object managed by
// embedders that owns the WebUI.

namespace webui {

// Initializes the embedding context for a given `tab_interface` and registers a
// necessary callback to keep this up to date for the life of a tab.
base::CallbackListSubscription InitEmbeddingContext(
    tabs::TabInterface* tab_interface);

// Called by embedders to set the browser for `host_contents`.
void SetBrowserWindowInterface(
    content::WebContents* host_contents,
    BrowserWindowInterface* browser_window_interface);

// Called by embedders to set the tab interface for `host_contents`. A tab is
// implicitly hosted in a browser and this setter also configures tracking of
// the associated browser, there is no need to call SetBrowserWindowInterface().
void SetTabInterface(content::WebContents* host_contents,
                     tabs::TabInterface* tab_interface);

// Gets the interface for the embedding browser. Clients should not persist this
// reference. May be null.
BrowserWindowInterface* GetBrowserWindowInterface(
    content::WebContents* host_contents);

// Gets the interface for the embedding tab. Clients should not persist this
// reference. May be null.
tabs::TabInterface* GetTabInterface(content::WebContents* host_contents);

// The associated browser window interface can be updated or nullified as the
// embedding context changes (e.g. a WebContents hosting a WebUI is added to,
// removed from or transferred to a browser window). Clients should register a
// context-changed subscription and respond to these events appropriately.
base::CallbackListSubscription RegisterBrowserWindowInterfaceChanged(
    content::WebContents* host_contents,
    base::RepeatingClosure context_changed_cb);

// The associated tab interface may be updated or nullified as the embedding
// context changes. Clients should register a context-changed subscription and
// respond to these events appropriately.
base::CallbackListSubscription RegisterTabInterfaceChanged(
    content::WebContents* host_contents,
    base::RepeatingClosure context_changed_cb);

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_EMBEDDING_CONTEXT_H_
