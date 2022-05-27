// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/cr_components/chromeos/network/network_proxy_input.m.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkProxyInputTest', function() {
  /** @type {!NetworkProxyInput|undefined} */
  let proxyInput;

  setup(function() {
    proxyInput = document.createElement('network-proxy-input');
    document.body.appendChild(proxyInput);
    flush();
  });

  test('Editable inputs', function() {
    assertFalse(proxyInput.editable);
    assertTrue(proxyInput.$$('#host').readonly);
    assertTrue(proxyInput.$$('#port').readonly);

    proxyInput.editable = true;
    flush();

    assertFalse(proxyInput.$$('#host').readonly);
    assertFalse(proxyInput.$$('#port').readonly);
  });
});
