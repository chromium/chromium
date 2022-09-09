// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', async function(event) {
    let permissionState = await navigator.permissions.query({name:'geolocation'})
    permissionState.onchange = function() {}
});