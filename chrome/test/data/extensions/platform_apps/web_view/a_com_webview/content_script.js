// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (document.body.childElementCount > 0)
  chrome.test.sendMessage('WebViewTest.UNKNOWN_ELEMENT_INJECTED');
else
  chrome.test.sendMessage('WebViewTest.NO_ELEMENT_INJECTED');
