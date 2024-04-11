// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/print_preview_cros_app_controller.js';

import {PrintPreviewCrosAppController} from 'chrome://os-print/js/print_preview_cros_app_controller.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('PrintPreviewCrosAppController', () => {
  let controller: PrintPreviewCrosAppController;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    controller = new PrintPreviewCrosAppController();
  });

  // Verify controller is an event target.
  test('controller is an event target', () => {
    assertTrue(
        controller instanceof EventTarget, 'Controller is an event target');
  });
});
