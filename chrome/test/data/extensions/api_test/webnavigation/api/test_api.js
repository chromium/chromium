// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.webNavigation.onBeforeNavigate.addListener(
    function(details) {});
chrome.webNavigation.onCommitted.addListener(
    function(details) {});
chrome.webNavigation.onDOMContentLoaded.addListener(
    function(details) {});
chrome.webNavigation.onCompleted.addListener(
    function(details) {});
chrome.webNavigation.onErrorOccurred.addListener(
    function(details) {});
chrome.webNavigation.onCreatedNavigationTarget.addListener(
    function(details) {});
chrome.test.notifyPass();
