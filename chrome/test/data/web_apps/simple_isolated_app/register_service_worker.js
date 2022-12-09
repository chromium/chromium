// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  const policy = trustedTypes.createPolicy('default', {
    createScriptURL: url => url,
  });

  navigator.serviceWorker.register(
    policy.createScriptURL('/service_worker.js'),
    // Match the scope defined in manifest_isolated.json. To be able to register
    // the worker for this scope, the 'Service-Worker-Allowed' header must be
    // set to '/' on the service worker script.
    { scope: '/' }
  );
})();
