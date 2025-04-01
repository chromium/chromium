// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Saves all launch urls consumed by this document.
var launchParamsTargetUrls = [];

window.launchQueue.setConsumer((launchParams) => {
    console.log('Got launch at ' + launchParams.targetURL);
    launchParamsTargetUrls.push(launchParams.targetURL)
});