// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.\
//    OnSelectionChange_NothingSelectedOnLoadingScreenSelection

const readAnythingApp = document.querySelector('read-anything-app').shadowRoot;
const emptyState = readAnythingApp.getElementById('empty-state-container');

let selectionChanged = false;
chrome.readAnything.onSelectionChange = function(
    _anchorNodeId, _anchorOffset, _focusNodeId, _focusOffset) {
  selectionChanged = true;
};

const range = new Range();
range.setStartBefore(emptyState);
range.setEndAfter(emptyState);
const selection = readAnythingApp.getSelection();
selection.removeAllRanges();
selection.addRange(range);

setTimeout(() => {
  domAutomationController.send(!selectionChanged);
}, 1000);
