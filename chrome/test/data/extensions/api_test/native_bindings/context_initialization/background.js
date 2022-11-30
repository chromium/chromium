// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let i = document.createElement('iframe');
i.src = chrome.runtime.getURL('iframe.html');
document.body.appendChild(i);
// Before the child frame has a chance to load (and thus before the
// ScriptContext is initialized), execute code in the child frame that
// initializes an API in the main frame (this frame). The contextMenus API
// requires JS initialization, which requires a JSRunner be instantiated for
// the context. If the new context (which is uninitialized and has no JSRunner)
// is used, this will fail. Instead, it should execute in this frame's context.
// See https://crbug.com/819968 for more details.
// IMPORTANT: For this test to be valid, the API being initialized (here,
// contextMenus), must have custom JS bindings.
i.contentWindow.eval('parent.chrome.contextMenus;');
chrome.test.notifyPass();
