// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {JsNetErrorCode} from 'chrome-untrusted://projector/ash/webui/projector_app/public/mojom/projector_types.mojom-webui.js';
import {browserProxy} from 'chrome-untrusted://projector/untrusted_projector_browser_proxy.js';
import {assertDeepEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('UntrustedProjectorBrowserProxyTest', () => {
  test('sendXhrSuccess', async function() {
    const response = await browserProxy.sendXhr(
        /*url=*/ 'https://www.googleapis.com/drive/v3/files/',
        /*method:*/ 'GET',
        /*requestBody=*/ '', /*useCredentials=*/ false, /*useApiKey=*/ true,
        /*headers=*/ {'X-Projector-Test': 'true'}, /*accountEmail=*/ '');

    assertDeepEquals(response, {
      success: true,
      response: '',
      error: '',
      errorCode: JsNetErrorCode.kNoError,
    });
  });
});
