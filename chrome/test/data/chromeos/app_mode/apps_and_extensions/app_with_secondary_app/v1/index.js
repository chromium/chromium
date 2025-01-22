// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const minimumChromeVersionAppId = 'ckgconpclkocfoolbepdpgmgaicpegnp';

window.addEventListener('load', function onload() {
  chrome.test.sendMessage('appWindowLoaded');

  const version = document.getElementById('secondary_app_version');
  version.innerHTML = 'WAITING RESPONSE..';

  chrome.runtime.sendMessage(
    minimumChromeVersionAppId,
    'GET_APP_VERSION',
    (response) => {
      version.innerHTML =
        response.version ? response.version : 'NULL RESPONSE';
    }
  );
});
