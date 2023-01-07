// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Open a panel, popup and tab to non-extension content.
chrome.windows.create({url: "about:blank", type: "panel"});
window.open("about:blank",
    "content_popup_non_extension", "height=200,width=200");
window.open("about:blank", "", "");

// Open a panel, popup and tab to extension content.
chrome.windows.create({url: "content_panel.html", type: "panel"});
chrome.windows.create({url: "content_popup.html", type: "popup"});
chrome.tabs.create({url: "content_tab.html"});
