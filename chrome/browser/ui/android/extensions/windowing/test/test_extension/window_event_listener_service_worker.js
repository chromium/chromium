// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log(`Start adding window event listeners...`);

chrome.windows.onBoundsChanged.addListener(window => {
    console.log(`Window bounds changed: ${JSON.stringify(window)}`);
});
chrome.windows.onCreated.addListener(window => {
    console.log(`Window created: ${JSON.stringify(window)}`);
});
chrome.windows.onFocusChanged.addListener(windowId => {
    console.log(`Window focus changed - newly focused window: ${windowId}`);
});
chrome.windows.onRemoved.addListener(windowId => {
    console.log(`Window removed: ${windowId}`);
});

console.log(`Window event listeners added.`);
