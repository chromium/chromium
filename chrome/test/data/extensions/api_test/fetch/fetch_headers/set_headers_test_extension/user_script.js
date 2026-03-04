// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  const allowedUrl = new URL('/fetch_allowed.html', location.href).href;
  await fetch(
      allowedUrl, {method: 'GET', headers: {'Content-Type': 'text/testing'}});

  const fetchAndSetForbiddenHeaderUrl =
      new URL('/fetch_forbidden.html?test=fetchAndSet', location.href).href;
  await fetch(
      fetchAndSetForbiddenHeaderUrl,
      {method: 'GET', headers: {'Origin': 'fakescheme://fakehostname'}});

  const fetchPostAndSetForbiddenHeaderUrl =
      new URL('/fetch_forbidden.html?test=fetchPostAndSet', location.href).href;
  await fetch(fetchPostAndSetForbiddenHeaderUrl, {
    method: 'POST',
    headers: {'Origin': 'fakescheme://fakehostname'},
    body: 'hello'
  });

  const headersApiSetForbiddenHeaderUrl =
      new URL('/fetch_forbidden.html?test=headersApiSet', location.href).href;
  const req1 = new Request(headersApiSetForbiddenHeaderUrl, {method: 'GET'});
  req1.headers.set('Origin', 'fakescheme://fakehostname');
  await fetch(req1);

  const headersApiRemoveForbiddenHeaderUrl =
      new URL('/fetch_forbidden.html?test=headersApiRemove', location.href)
          .href;
  const req2 = new Request(
      headersApiRemoveForbiddenHeaderUrl,
      {method: 'GET', headers: {'Origin': 'fakescheme://fakehostname'}});
  req2.headers.delete('Origin');
  await fetch(req2);

  chrome.test.sendMessage('user_script_fetches_completed');
})();
