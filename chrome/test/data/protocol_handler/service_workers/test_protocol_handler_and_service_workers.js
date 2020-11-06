// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests
// --gtest_filter=ProtocolHandlerBrowserTest.*

async function registerFetchListenerForHTMLHandler() {
  await navigator.serviceWorker.register(
      '/protocol_handler/service_workers/fetch_listener_for_html_handler.js');
  await navigator.serviceWorker.ready;
  return true;
}

function absoluteURL(path) {
  return `${location.origin}/protocol_handler/service_workers/${path}`;
}

function registerHTMLHandler() {
  navigator.registerProtocolHandler(
      'web+html', absoluteURL('handler.html?url=%s'), 'title');
}

async function handledByServiceWorker(url) {
  const a = document.body.appendChild(document.createElement('a'));
  a.href = url;
  a.rel = 'opener';
  a.target = '_blank';
  let handled_by_service_worker;
  await new Promise(resolve => {
    window.addEventListener('message', function(event) {
      handled_by_service_worker = event.data.handled_by_service_worker;
      event.source.close();
      resolve();
    }, {once: true})
    a.click();
  });
  return handled_by_service_worker;
}

function pageWithCustomSchemeHandledByServiceWorker() {
  return handledByServiceWorker('web+html:path');
}
