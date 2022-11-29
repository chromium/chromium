// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingChooseWipeDevicePage} from 'chrome://shimless-rma/onboarding_choose_wipe_device_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('onboardingChooseWipeDevicePageTest', function() {
  /** @type {?OnboardingChooseWipeDevicePage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = '';
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeChooseWipeDevicePage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingChooseWipeDevicePage} */ (
        document.createElement('onboarding-choose-wipe-device-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ChooseWipeDevicePageInitializes', async () => {
    await initializeChooseWipeDevicePage();

    assertTrue(!!component);
  });

  test('ChooseWipeDevicePageSelectWipeDevice', async () => {
    const resolver = new PromiseResolver();
    await initializeChooseWipeDevicePage();

    let shouldWipeDevice = false;
    service.setWipeDevice = (wipeDevice) => {
      shouldWipeDevice = wipeDevice;
      return resolver.promise;
    };

    const wipeDeviceOption =
        component.shadowRoot.querySelector('cr-radio-button[name=wipeDevice]');
    wipeDeviceOption.click();
    assertTrue(wipeDeviceOption.checked);

    component.onNextButtonClick();
    await resolver;
    assertTrue(shouldWipeDevice);
  });

  test('ChooseWipeDevicePageSelectPreserveData', async () => {
    const resolver = new PromiseResolver();
    await initializeChooseWipeDevicePage();

    let shouldWipeDevice = true;
    service.setWipeDevice = (wipeDevice) => {
      shouldWipeDevice = wipeDevice;
      return resolver.promise;
    };

    const wipeDeviceOption = component.shadowRoot.querySelector(
        'cr-radio-button[name=preserveData]');
    wipeDeviceOption.click();
    assertTrue(wipeDeviceOption.checked);

    component.onNextButtonClick();
    await resolver;
    assertFalse(shouldWipeDevice);
  });
});
