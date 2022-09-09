// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  document.write('bypass');
} catch (e) {
}
const bypassSucess = document.body.innerText.indexOf('bypass') != -1;
chrome.runtime.connect().postMessage(bypassSucess);
