// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_dropdown_controller.js';

import {DestinationDropdownController} from 'chrome://os-print/js/destination_dropdown_controller.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('DestinationDropdownController', () => {
  let controller: DestinationDropdownController;

  setup(() => {
    controller = new DestinationDropdownController();
  });

  // Verify controller can be constructed.
  test('controller is an event target', () => {
    assertTrue(
        controller instanceof EventTarget, 'Controller is an event target');
  });
});
