// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_row_controller.js';

import {DestinationRowController} from 'chrome://os-print/js/destination_row_controller.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('DestinationRowController', () => {
  let controller: DestinationRowController;

  setup(() => {
    controller = new DestinationRowController();
  });

  // Verify controller is an EventTarget.
  test('controller is an event target', () => {
    assertTrue(
        controller instanceof EventTarget, 'Controller is an event target');
  });
});
