// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.sendMessage(location.href);

// Inject speculation rules if the page is the initial test page.
if (location.pathname == '/empty.html') {
  // Generate the prerendering target URL that is in the same origin with
  // the path '/title1.html'.
  const target = new URL(location.href);
  target.pathname = "/title1.html";

  // Create a script tag, and inject it.
  const script = document.createElement('script');
  script.type = 'speculationrules';
  script.innerText =
    `{ "prerender": [ { "source": "list", "urls": [ "${target.href}" ]} ] }`;
  document.head.appendChild(script);
}
