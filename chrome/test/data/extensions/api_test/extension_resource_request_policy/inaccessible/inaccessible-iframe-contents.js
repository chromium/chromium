// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('here');
chrome.test.notifyFail('Iframe from another extension not listed in ' +
    'web_accessible_resources should not have been loaded');
