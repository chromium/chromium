// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
const metaTags = document.getElementsByTagName('meta');
const output = {};
for (let i = 0; i < metaTags.length; i++) {
  const curMeta = metaTags[i];
  const name = curMeta.getAttribute('property');
  const value = curMeta.getAttribute('content');
  if (!name || !value) {
    continue;
  }
  // "og" in this context refers to "open graph" rather than "optimization
  // guide".
  if (name.startsWith('og:')) {
    output[name.substring(3)] = value;
  }
}
return JSON.stringify(output);
})();
