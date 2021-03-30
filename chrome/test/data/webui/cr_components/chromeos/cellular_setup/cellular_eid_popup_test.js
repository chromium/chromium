// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_eid_popup.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {assertTrue, assertEquals, assertDeepEquals} from '../../../chai_assert.js';
// #import {FakeESimManagerRemote} from './fake_esim_manager_remote.m.js';
// #import {FakeCanvasContext} from "./fake_canvas_context.m.js";
// clang-format on

suite('CrComponentsCellularEidPopupTest', function() {
  let eSimManagerRemote;
  let testEuicc;
  let eidPopup;
  let canvasContext;

  function init(eidQRCode) {
    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);
    testEuicc = eSimManagerRemote.addEuiccForTest(1);
    if (eidQRCode) {
      testEuicc.setEidQRCodeForTest(eidQRCode);
    }

    eidPopup = document.createElement('cellular-eid-popup');
    eidPopup.euicc = testEuicc;
    canvasContext = new cellular_setup.FakeCanvasContext();
    eidPopup.setCanvasContextForTest(canvasContext);
    document.body.appendChild(eidPopup);

    // Flush and wait for next macrotask.
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Should display EID', async function() {
    await init();
    assertEquals(
        testEuicc.properties.eid, eidPopup.$$('#eid').textContent.trim());
  });

  test('Should render EID QRCode', async function() {
    await init({size: 2, data: [1, 0, 0, 1]});
    const qrCodeCanvas = eidPopup.$$('#qrCodeCanvas');
    assertEquals(qrCodeCanvas.width, 50);
    assertEquals(qrCodeCanvas.height, 50);
    assertDeepEquals(canvasContext.getClearRectCalls(), [[0, 0, 50, 50]]);
    assertDeepEquals(
        canvasContext.getFillRectCalls(), [[20, 20, 5, 5], [25, 25, 5, 5]]);
  });
});