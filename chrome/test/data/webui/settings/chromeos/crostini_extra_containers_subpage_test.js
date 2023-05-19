// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrostiniBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

suite('CrostiniExtraContainersSubpageTests', function() {
  /** @type {?SettingsCrostiniPageElement} */
  let crostiniPage = null;

  /** @type {?TestCrostiniBrowserProxy} */
  let crostiniBrowserProxy = null;

  /** @type {?SettingsCrostinuExtraContainersElement} */
  let subpage;

  setup(async function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();

    const allContainers_ = [
      {
        'id': {'container_name': 'penguin', 'vm_name': 'termina'},
      },
      {
        'id': {'container_name': 'custom_container_1', 'vm_name': 'termina'},
      },
      {
        'id':
            {'container_name': 'custom_container_2', 'vm_name': 'not_termina'},
      },
    ];

    const sharedVmDevices_ = [
      {
        id: allContainers_[0].id,
        vmDevices: {microphone: true},
      },
      {
        id: allContainers_[1].id,
        vmDevices: {microphone: false},
      },
      {
        id: allContainers_[2].id,
        vmDevices: {microphone: true},
      },
    ];

    crostiniBrowserProxy.containerInfo = allContainers_;
    crostiniBrowserProxy.sharedVmDevices = sharedVmDevices_;
    crostiniPage.prefs = {
      crostini: {
        enabled: {value: true},
      },
    };
    flush();
    assertEquals(0, crostiniBrowserProxy.getCallCount('requestContainerInfo'));
    assertEquals(
        0, crostiniBrowserProxy.getCallCount('requestSharedVmDevices'));

    Router.getInstance().navigateTo(
        routes.CROSTINI_EXTRA_CONTAINERS);

    await flushTasks();
    subpage = crostiniPage.shadowRoot.querySelector(
        'settings-crostini-extra-containers');
    assertTrue(!!subpage);
    assertEquals(1, crostiniBrowserProxy.getCallCount('requestContainerInfo'));
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('requestSharedVmDevices'));
  });

  teardown(function() {
    crostiniPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('CreateContainerDialog', function() {
    /** @type {?Element} */
    let containerNameInput;

    /** @type {?Element} */
    let vmNameInput;

    /** @type {?Element} */
    let createButton;

    /** @type {?Element} */
    let advancedToggle;

    /** @type {?Element} */
    let advancedSection;

    setup(async function() {
      subpage.shadowRoot.querySelector('#create').click();

      await flushTasks();
      subpage = subpage.shadowRoot.querySelector(
          'settings-crostini-create-container-dialog');

      containerNameInput = subpage.root.querySelector('#containerNameInput');
      vmNameInput = subpage.root.querySelector('#vmNameInput');
      createButton = subpage.root.querySelector('#create');
      advancedToggle = subpage.root.querySelector('#advancedToggle');
      advancedSection = subpage.root.querySelector('.advanced-section');
    });

    /**
     * Helper function to enter |inputValue| in the element |input| and fire an
     * input event.
     * @param {!Element} inputElement
     * @param {string} inputValue
     */
    function setInput(inputElement, inputValue) {
      inputElement.value = inputValue;
      inputElement.dispatchEvent(new Event('input'));
    }

    /**
     * Helper function to check that the containerNameInput is valid and
     * createButton is enabled.
     */
    function assertValidAndEnabled() {
      assertFalse(containerNameInput.invalid);
      assertFalse(createButton.disabled);
    }

    /**
     * Helper function to check that the containerNameInput is invalid with
     * |errorMsgName|, and createButton is disabled.
     * @param {string} errorMsg
     */
    function assertInvalidAndDisabled(errorMsgName) {
      assertTrue(containerNameInput.invalid);
      assertTrue(createButton.disabled);
      assertEquals(
          containerNameInput.errorMessage,
          loadTimeData.getString(errorMsgName));
    }

    test('AddContainerValidInDefaultVm', async function() {
      setInput(containerNameInput, 'custom_container_2');
      assertValidAndEnabled();

      createButton.click();
      assertEquals(1, crostiniBrowserProxy.getCallCount('createContainer'));
    });

    test('AddContainerValidInNonDefaultVm', async function() {
      setInput(containerNameInput, 'custom_container_1');
      setInput(vmNameInput, 'not_termina');
      assertValidAndEnabled();

      createButton.click();
      assertEquals(1, crostiniBrowserProxy.getCallCount('createContainer'));
    });

    test(
        'ErrorAndDisabledCreateForDefaultContainerNameInDefaultVm',
        async function() {
          setInput(containerNameInput, 'penguin');

          assertInvalidAndDisabled(
              'crostiniExtraContainersCreateDialogContainerExistsError');
        });

    test(
        'ErrorAndDisabledCreateForDefaultContainerNameInNonDefaultVm',
        async function() {
          setInput(containerNameInput, 'penguin');
          setInput(vmNameInput, 'not_termina');

          assertInvalidAndDisabled(
              'crostiniExtraContainersCreateDialogContainerExistsError');
        });

    test(
        'ErrorAndDisabledCreateForDuplicateContainerNameInDefaultVm',
        async function() {
          setInput(containerNameInput, 'custom_container_1');

          assertInvalidAndDisabled(
              'crostiniExtraContainersCreateDialogContainerExistsError');
        });

    test(
        'ErrorAndDisabledCreateForDuplicateContainerNameInNonDefaultVm',
        async function() {
          setInput(containerNameInput, 'custom_container_2');
          setInput(vmNameInput, 'not_termina');

          assertInvalidAndDisabled(
              'crostiniExtraContainersCreateDialogContainerExistsError');
        });

    test(
        'ErrorAndDisabledCreateForEmptyContainerNameInDefaultVm',
        async function() {
          setInput(containerNameInput, '');

          assertInvalidAndDisabled(
              'crostiniExtraContainersCreateDialogEmptyContainerNameError');
        });

    test(
        'ErrorAndDisabledCreateForEmptyContainerNameInNonDefaultVm',
        async function() {
          setInput(containerNameInput, '');
          setInput(vmNameInput, 'not_termina');

          assertInvalidAndDisabled(
              'crostiniExtraContainersCreateDialogEmptyContainerNameError');
        });

    test('ReenabledButtonAfterError', async function() {
      setInput(containerNameInput, 'penguin');
      assertInvalidAndDisabled(
          'crostiniExtraContainersCreateDialogContainerExistsError');

      setInput(containerNameInput, 'custom_container_2');
      assertValidAndEnabled();

      createButton.click();
      assertEquals(1, crostiniBrowserProxy.getCallCount('createContainer'));
    });

    test('CreateContainerAdvancedWithFile', async function() {
      setInput(containerNameInput, 'advanced_container');
      setInput(vmNameInput, 'termina');

      assertTrue(advancedSection.hidden);
      advancedToggle.click();
      assertFalse(advancedSection.hidden);

      const containerFileInput =
          subpage.root.querySelector('#containerFileInput');
      setInput(containerFileInput, 'test_backup.tini');
      assertValidAndEnabled();

      createButton.click();
      assertEquals(1, crostiniBrowserProxy.getCallCount('createContainer'));
      const args = crostiniBrowserProxy.getArgs('createContainer')[0];
      assertArrayEquals(
          [
            {vm_name: 'termina', container_name: 'advanced_container'},
            '',
            '',
            'test_backup.tini',
          ],
          args);
    });
  });

  suite('ExportImportContainer', function() {
    test('Export', async function() {
      subpage.shadowRoot.querySelector('#showContainerMenu1').click();

      await flushTasks();
      assertTrue(!!subpage.shadowRoot.querySelector('#exportContainerButton'));
      subpage.shadowRoot.querySelector('#exportContainerButton').click();
      const args = crostiniBrowserProxy.getArgs('exportCrostiniContainer');
      assertEquals(1, args.length);
      assertEquals(args[0].vm_name, 'termina');
      assertEquals(args[0].container_name, 'custom_container_1');
    });

    test('Import', async function() {
      subpage.shadowRoot.querySelector('#showContainerMenu1').click();

      await flushTasks();
      assertTrue(!!subpage.shadowRoot.querySelector('#importContainerButton'));
      subpage.shadowRoot.querySelector('#importContainerButton').click();
      const args = crostiniBrowserProxy.getArgs('importCrostiniContainer');
      assertEquals(1, args.length);
      assertEquals(args[0].vm_name, 'termina');
      assertEquals(args[0].container_name, 'custom_container_1');
    });

    test('ExportImportButtonsGetDisabledOnOperationStatus', async function() {
      subpage.shadowRoot.querySelector('#showContainerMenu1').click();

      await flushTasks();
      assertFalse(
          subpage.shadowRoot.querySelector('#exportContainerButton').disabled);
      assertFalse(
          subpage.shadowRoot.querySelector('#importContainerButton').disabled);
      webUIListenerCallback(
          'crostini-export-import-operation-status-changed', true);

      await flushTasks();
      assertTrue(
          subpage.shadowRoot.querySelector('#exportContainerButton').disabled);
      assertTrue(
          subpage.shadowRoot.querySelector('#importContainerButton').disabled);
      webUIListenerCallback(
          'crostini-export-import-operation-status-changed', false);

      await flushTasks();
      assertFalse(
          subpage.shadowRoot.querySelector('#exportContainerButton').disabled);
      assertFalse(
          subpage.shadowRoot.querySelector('#importContainerButton').disabled);
    });

    test(
        'ExportImportButtonsDisabledOnWhenInstallingCrostini',
        async function() {
          subpage.shadowRoot.querySelector('#showContainerMenu1').click();

          await flushTasks();
          assertFalse(subpage.shadowRoot.querySelector('#exportContainerButton')
                          .disabled);
          assertFalse(subpage.shadowRoot.querySelector('#importContainerButton')
                          .disabled);
          webUIListenerCallback('crostini-installer-status-changed', true);

          await flushTasks();
          assertTrue(subpage.shadowRoot.querySelector('#exportContainerButton')
                         .disabled);
          assertTrue(subpage.shadowRoot.querySelector('#importContainerButton')
                         .disabled);
          webUIListenerCallback('crostini-installer-status-changed', false);

          await flushTasks();
          assertFalse(subpage.shadowRoot.querySelector('#exportContainerButton')
                          .disabled);
          assertFalse(subpage.shadowRoot.querySelector('#importContainerButton')
                          .disabled);
        });
  });

  suite('ContainerDetails', function() {
    test('ExpandButton', async function() {
      const expandButton =
          subpage.shadowRoot.querySelector('#expand-button-termina-penguin');
      assertTrue(!!expandButton);

      // The collapse element should open/close on clicking |expandButton|.
      const collapse =
          subpage.shadowRoot.querySelector('#collapse-termina-penguin');
      assertTrue(!!collapse);

      assertFalse(collapse.opened);
      expandButton.click();
      await flushTasks();
      assertTrue(collapse.opened);

      expandButton.click();
      await flushTasks();
      assertFalse(collapse.opened);
    });

    test('ToggleMicrophoneOff', async function() {
      // The toggle is inside an iron-collapse, but we can still click it
      // via the testing apis.
      const toggle =
          subpage.shadowRoot.querySelector('#microphone-termina-penguin');

      assertTrue(!!toggle);
      assertTrue(toggle.checked);

      toggle.click();
      await crostiniBrowserProxy.resolvePromises('setVmDeviceShared', true);
      await crostiniBrowserProxy.resolvePromises('isVmDeviceShared', false);

      assertFalse(toggle.checked);

      assertEquals(1, crostiniBrowserProxy.getCallCount('setVmDeviceShared'));
      const args1 = crostiniBrowserProxy.getArgs('setVmDeviceShared')[0];
      assertArrayEquals(
          [
            {vm_name: 'termina', container_name: 'penguin'},
            'microphone',
            false,
          ],
          args1);

      assertEquals(1, crostiniBrowserProxy.getCallCount('isVmDeviceShared'));
      const args2 = crostiniBrowserProxy.getArgs('isVmDeviceShared')[0];
      assertArrayEquals(
          [
            {vm_name: 'termina', container_name: 'penguin'},
            'microphone',
          ],
          args2);
    });
  });
});
