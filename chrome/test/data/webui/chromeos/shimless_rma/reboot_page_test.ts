// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {RebootPage} from 'chrome://shimless-rma/reboot_page.js';
import {RmadErrorCode} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('rebootPageTest', function() {
  let component: RebootPage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
  });

  function initializeRebootPage(): Promise<void> {
    assert(!component);
    component = document.createElement(RebootPage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the page initializes and renders.
  test('ComponentRenders', async () => {
    await initializeRebootPage();

    assert(component);
    const basePage =
        strictQuery('base-page', component.shadowRoot, HTMLElement);
    assert(basePage);
  });

  // Verify the text content updates based on the error code.
  test('ErrorCodeUpdatesPage', async () => {
    await initializeRebootPage();

    assert(component);
    component.errorCode = RmadErrorCode.kExpectReboot;

    // The displayed value for how many seconds to wait before the reboot or
    // shutdown.
    const delayDuration = 3;
    const title = strictQuery('#title', component.shadowRoot, HTMLElement);
    const instructions =
        strictQuery('#instructions', component.shadowRoot, HTMLElement);
    assertEquals(
        loadTimeData.getString('rebootPageTitle'), title.textContent!.trim());
    assertEquals(
        loadTimeData.getStringF('rebootPageMessage', delayDuration),
        instructions.textContent!.trim());

    component.errorCode = RmadErrorCode.kExpectShutdown;
    assertEquals(
        loadTimeData.getString('shutdownPageTitle'), title.textContent!.trim());
    assertEquals(
        loadTimeData.getStringF('shutdownPageMessage', delayDuration),
        instructions.textContent!.trim());
  });
});
