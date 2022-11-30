// Copyright 2021 The Chromium Authors
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

  setWindowControlsOverlayGeometryChange(document.title);
}

function startWorker(worker, options) {
  navigator.serviceWorker.register(worker, options);
}

// For this to work, the linked manifest.json must contain
// "display_override": ["window-controls-overlay"].
function setWindowControlsOverlayGeometryChange(siteTitle) {
  document.title = siteTitle;
  if (navigator.windowControlsOverlay) {
    navigator.windowControlsOverlay.ongeometrychange = (e) => {
      document.title =
        navigator.windowControlsOverlay.visible
          ? siteTitle + ": WCO Enabled"
          : siteTitle;
    }
  }
}