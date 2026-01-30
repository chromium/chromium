// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.sidePanel.setPanelBehavior({openPanelOnActionClick: true})
    .catch(
        (error) => {return chrome.test.notifyFail(
            error ? error.toString() : 'Unknown error')})
    .then(() => chrome.test.sendMessage('running'));
