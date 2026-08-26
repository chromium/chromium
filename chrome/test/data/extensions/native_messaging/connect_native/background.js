// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The port should be active and connected to the test app for as long as the
// service worker remains active.
globalThis.port =
    chrome.runtime.connectNative('org.chromium.chrome.tests.support');
