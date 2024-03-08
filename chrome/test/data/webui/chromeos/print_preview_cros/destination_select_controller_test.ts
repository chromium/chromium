// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_select_controller.js';

import {DestinationSelectController} from 'chrome://os-print/js/destination_select_controller.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('DestinationSelectController', () => {
  let controller: DestinationSelectController;

  setup(() => {
    controller = new DestinationSelectController();
    assertTrue(!!controller);
  });

  // Verify controller is event target.
  test('is event target', () => {
    assertTrue(controller instanceof EventTarget, 'Is event target');
  });

  // Verify shouldShowLoading returns true by default.
  test('shouldShowLoading returns true by default', () => {
    assertTrue(controller.shouldShowLoading());
  });
});
