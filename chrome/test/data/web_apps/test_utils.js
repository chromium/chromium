// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addManifestLinkTag(optionalCustomUrl) {
  const url = new URL(window.location.href);
  let manifestUrl = url.searchParams.get('manifest');
  if (!manifestUrl) {
    manifestUrl = optionalCustomUrl || 'basic.json';
  }

  var linkTag = document.createElement("link");
  linkTag.id = "manifest";
  linkTag.rel = "manifest";
  linkTag.href = `./${manifestUrl}`;
  document.head.append(linkTag);
}

function startWorker(worker) {
  navigator.serviceWorker.register(worker);
}
