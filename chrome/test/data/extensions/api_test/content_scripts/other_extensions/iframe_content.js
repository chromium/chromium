// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('load', function() {
  const parentExtensionPage = unescape(location.hash.replace('#', ''));

  console.log(`PAGE: Sending content to parent extension page - ${
      parentExtensionPage}`);
  window.parent.postMessage(
      document.getElementById('content').innerText, parentExtensionPage);
});
