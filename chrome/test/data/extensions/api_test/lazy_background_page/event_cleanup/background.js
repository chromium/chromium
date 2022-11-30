// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onUpdated.addListener(function() {});

// This is just a fun way to keep the event page alive until we're ready to
// shut it down - the extensions system won't spin down a page while there's
// a pending response to an API call.
chrome.test.sendMessage('ready', function(reply) {});
