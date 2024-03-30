// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrRadioButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingChooseWipeDevicePage} from 'chrome://shimless-rma/onboarding_choose_wipe_device_page.js';
import {StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('onboardingChooseWipeDevicePageTest', function() {
  let component: OnboardingChooseWipeDevicePage|null = null;

  let service: FakeShimlessRmaService|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    service = null;
  });

  function initializeChooseWipeDevicePage(): Promise<void> {
    assert(!component);
    component = document.createElement(OnboardingChooseWipeDevicePage.is);
    assert(component);
    document.body.appendChild(component);
    return flushTasks();
  }

  // Verify the page is initialized.
  test('ChooseWipeDevicePageInitializes', async () => {
    await initializeChooseWipeDevicePage();
    assert(component);
  });

  // Verify the correct values are sent when "Wipe Device" is selected.
  test('ChooseWipeDevicePageSelectWipeDevice', async () => {
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    await initializeChooseWipeDevicePage();

    let shouldWipeDevice = false;
    assert(service);
    service.setWipeDevice = (wipeDevice: boolean) => {
      shouldWipeDevice = wipeDevice;
      return resolver.promise;
    };

    assert(component);
    const wipeDeviceOption = strictQuery(
        'cr-radio-button[name=wipeDevice]', component.shadowRoot,
        CrRadioButtonElement);
    wipeDeviceOption.click();
    assertTrue(wipeDeviceOption.checked);

    component.onNextButtonClick();
    await resolver;
    assertTrue(shouldWipeDevice);
  });

  // Verify the correct values are sent when "Preserve Data" is selected.
  test('ChooseWipeDevicePageSelectPreserveData', async () => {
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    await initializeChooseWipeDevicePage();

    let shouldWipeDevice = true;
    assert(service);
    service.setWipeDevice = (wipeDevice: boolean) => {
      shouldWipeDevice = wipeDevice;
      return resolver.promise;
    };

    assert(component);
    const wipeDeviceOption = strictQuery(
        'cr-radio-button[name=preserveData]', component.shadowRoot,
        CrRadioButtonElement);
    wipeDeviceOption.click();
    assertTrue(wipeDeviceOption.checked);

    component.onNextButtonClick();
    await resolver;
    assertFalse(shouldWipeDevice);
  });
});
