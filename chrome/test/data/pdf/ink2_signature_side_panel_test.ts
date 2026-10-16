// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupMockMetricsPrivate} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;

suite('Ink2SignatureSidePanelTest', () => {
  // Test that toggling signature mode opens the side panel.
  test('OpenSidePanel', async () => {
    const mockMetricsPrivate = setupMockMetricsPrivate();

    viewer.$.toolbar.setAnnotationMode(AnnotationMode.SIGNATURE);
    await microtasksFinished();

    assertEquals(AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);
    assertTrue(isVisible(
        viewer.shadowRoot.querySelector('viewer-signature-side-panel')));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 1);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 0);
    mockMetricsPrivate.assertCount(UserAction.ENTER_INK2_SIGNATURE_MODE, 1);
    mockMetricsPrivate.assertCount(UserAction.EXIT_INK2_SIGNATURE_MODE, 0);
  });
});
