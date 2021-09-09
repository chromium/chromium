// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Gets the query string from the URL.
 * For example, if the URL is chrome://projector/annotator/abc, then query is
 * "abc".
 */
function getQuery() {
  if (!document.location.pathname) {
    return '';
  }
  const paths = document.location.pathname.split('/');
  if (paths.length < 1) {
    return '';
  }
  return paths[paths.length - 1];
}

function init() {
  // TODO(b/197343976): embed chrome-untrusted://projector/app instead.
  document.body.querySelector('iframe').src =
      'chrome-untrusted://projector/' + getQuery();
}

document.addEventListener('DOMContentLoaded', init, false);
