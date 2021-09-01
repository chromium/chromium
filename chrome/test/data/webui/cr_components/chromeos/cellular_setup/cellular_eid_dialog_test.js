// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_eid_dialog.m.js';

// #import {afterNextRender, flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {assertTrue, assertEquals, assertDeepEquals} from '../../../chai_assert.js';
// #import {FakeESimManagerRemote} from './fake_esim_manager_remote.m.js';
// #import {FakeCanvasContext} from "./fake_canvas_context.m.js";
// #import {eventToPromise, flushTasks} from 'chrome://test/test_util.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.js';
// clang-format on

suite('CrComponentsCellularEidDialogTest', function() {
  let eSimManagerRemote;
  let testEuicc;
  let eidDialog;
  let canvasContext;

  function init(eidQRCode) {
    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);
    testEuicc = eSimManagerRemote.addEuiccForTest(1);
    if (eidQRCode) {
      testEuicc.setEidQRCodeForTest(eidQRCode);
    }

    eidDialog = document.createElement('cellular-eid-dialog');
    eidDialog.euicc = testEuicc;
    canvasContext = new cellular_setup.FakeCanvasContext();
    eidDialog.setCanvasContextForTest(canvasContext);
    document.body.appendChild(eidDialog);

    // Flush and wait for next macrotask.
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Should display EID', async function() {
    await init();
    assertEquals(
        testEuicc.properties.eid, eidDialog.$$('#eid').textContent.trim());
  });

  test('Should render EID QRCode', async function() {
    await init({size: 2, data: [1, 0, 0, 1]});
    const qrCodeCanvas = eidDialog.$$('#qrCodeCanvas');
    assertEquals(qrCodeCanvas.width, 10);
    assertEquals(qrCodeCanvas.height, 10);
    assertDeepEquals(canvasContext.getClearRectCalls(), [[0, 0, 10, 10]]);
    assertDeepEquals(
        canvasContext.getFillRectCalls(), [[0, 0, 5, 5], [5, 5, 5, 5]]);
  });

  test('should close EID when done is pressed', async function() {
    await init();
    assertTrue(eidDialog.$.eidDialog.open);

    eidDialog.$.done.click();
    Polymer.dom.flush();

    assertFalse(eidDialog.$.eidDialog.open);
  });
});
