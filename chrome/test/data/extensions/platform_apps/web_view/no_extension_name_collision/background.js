// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for https://crbug.com/1014385
// Since this is an extension's background page, the following lines should not
// throw a syntax error due to redeclaration.
const ExtensionOptions = {};
const AppView = {};
const WebView = {};
window.testPassed = true;
