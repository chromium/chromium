// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {populateFakeShimlessRmaService, setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessCustomElementType, StateComponentMapping} from 'chrome://shimless-rma/shimless_rma.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('allInputsDisabledTest', function() {
  const INPUT_TYPES =
      ['cr-input', 'cr-button', 'cr-radio-group', 'cr-slider', 'cr-toggle'];

  let service: FakeShimlessRmaService|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    service = new FakeShimlessRmaService();
    populateFakeShimlessRmaService(service);
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    service = null;
  });

  // Test that the set of inputs specified in |INPUT_TYPES| are disabled on each
  // page when |allButtonsDisabled| is set.
  test('AllInputsDisabled', async () => {
    Object.entries(StateComponentMapping).forEach(([_state, pageInfo]) => {
      const component = document.createElement(pageInfo.componentIs) as
          ShimlessCustomElementType;
      assert(component);
      document.body.appendChild(component);

      component.allButtonsDisabled = true;
      for (const inputType of INPUT_TYPES) {
        const inputElements = component.shadowRoot!.querySelectorAll(
                                  inputType) as NodeListOf<HTMLInputElement>;
        for (const inputElement of inputElements) {
          // Skip buttons in the dialogs because they're not expected to be
          // disabled.
          if (inputElement.closest('cr-dialog')) {
            continue;
          }

          assertTrue(
              inputElement.disabled,
              'Component: ' + component.nodeName +
                  ' has an undisabled input. Input Type: ' + inputType +
                  ' Id: ' + inputElement.id);
        }
      }

      document.body.removeChild(component);
    });
  });
});
