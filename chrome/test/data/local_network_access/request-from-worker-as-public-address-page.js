// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const params = new URLSearchParams(location.search);
const start_worker_manually = params.has('start_worker_manually');

let worker;
let resolve;
function start_worker() {
  worker = new Worker(
    params.has('script') ? params.get('script') : 'request-from-worker-as-public-address-worker.js');

  worker.onmessage = e => { resolve(e.data); };
}

function worker_request(url, method) {
  let p = new Promise(r => { resolve = r; });
  worker.postMessage({url, method});
  return p;
}

function fetch_from_worker(url) {
  return worker_request(url, 'fetch');
}

function webtransport_open_from_worker(url) {
  return worker_request(url, 'webtransport-open');
}

function webtransport_close_from_worker() {
  return worker_request('', 'webtransport-close');
}


if (!start_worker_manually) {
  start_worker();
}
