// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const params = new URLSearchParams(location.search);

fetch(params.get('href'), {mode: 'no-cors'}).then(() => {
  parent.postMessage('SUCCESS', '*');
}, () => {
  parent.postMessage('FAIL', '*');
});