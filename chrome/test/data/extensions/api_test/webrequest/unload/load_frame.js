// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const frameUrl = decodeURIComponent(location.search.slice(1));
console.assert(frameUrl, `Frame URL must be specified after '?'.`);

window.onload = function() {
  const f = document.createElement('iframe');
  f.src = frameUrl;
  document.body.appendChild(f);
};
