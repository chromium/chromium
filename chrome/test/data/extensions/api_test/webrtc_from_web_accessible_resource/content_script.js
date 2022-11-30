// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var iframe = document.createElement('iframe');
iframe.src = chrome.runtime.getURL(
    'iframe_content.html' + document.location.search);
iframe.allow = 'microphone';
document.body.appendChild(iframe);
