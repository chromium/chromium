// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/cellular_eid_dialog.js';

import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertDeepEquals, assertEquals, assertTrue} from '../../../chromeos/chai_assert.js';

import {FakeCanvasContext} from './fake_canvas_context.js';
import {FakeESimManagerRemote} from './fake_esim_manager_remote.js';

suite('CrComponentsCellularEidDialogTest', function() {
  let eSimManagerRemote;
  let testEuicc;
  let eidDialog;
  let canvasContext;

  function init(eidQRCode) {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);
    testEuicc = eSimManagerRemote.addEuiccForTest(1);
    if (eidQRCode) {
      testEuicc.setEidQRCodeForTest(eidQRCode);
    }

    eidDialog = document.createElement('cellular-eid-dialog');
    eidDialog.euicc = testEuicc;
    canvasContext = new FakeCanvasContext();
    eidDialog.setCanvasContextForTest(canvasContext);
    document.body.appendChild(eidDialog);

    // Flush and wait for next macrotask.
    flush();
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
    flush();

    assertFalse(eidDialog.$.eidDialog.open);
  });
});
