// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ContainerInfo, CrostiniBrowserProxyImpl, ExtraContainersCreateDialog, ExtraContainersElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement, CrToggleElement, IronCollapseElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

import {SharedVmDevices, TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

suite('<settings-crostini-extra-containers>', () => {
  let crostiniBrowserProxy: TestCrostiniBrowserProxy;
  let subpage: ExtraContainersElement;

  setup(async () => {
    const allContainers: ContainerInfo[] = [
      {
        id: {container_name: 'penguin', vm_name: 'termina'},
        ipv4: null,
      },
      {
        id: {container_name: 'custom_container_1', vm_name: 'termina'},
        ipv4: null,
      },
      {
        id: {container_name: 'custom_container_2', vm_name: 'not_termina'},
        ipv4: null,
      },
    ];

    const sharedVmDevices: SharedVmDevices[] = [
      {
        id: allContainers[0]!.id,
        vmDevices: {microphone: true},
      },
      {
        id: allContainers[1]!.id,
        vmDevices: {microphone: false},
      },
      {
        id: allContainers[2]!.id,
        vmDevices: {microphone: true},
      },
    ];

    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    crostiniBrowserProxy.containerInfo = allContainers;
    crostiniBrowserProxy.sharedVmDevices = sharedVmDevices;
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);

    Router.getInstance().navigateTo(routes.CROSTINI_EXTRA_CONTAINERS);

    clearBody();
    subpage = document.createElement('settings-crostini-extra-containers');
    subpage.prefs = {
      crostini: {
        enabled: {value: true},
      },
    };
    document.body.appendChild(subpage);
    await flushTasks();

    assertEquals(1, crostiniBrowserProxy.getCallCount('requestContainerInfo'));
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('requestSharedVmDevices'));
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  suite('CreateContainerDialog', () => {
    let createDialogSubpage: ExtraContainersCreateDialog;
    let containerNameInput: CrInputElement;
    let vmNameInput: CrInputElement;
    let createButton: HTMLButtonElement;
    let advancedToggle: HTMLButtonElement;
    let advancedSection: HTMLElement;

    setup(async () => {
      const button =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>('#create');
      assertTrue(!!button);
      button.click();

      await flushTasks();
      const dialogElement =
          subpage.shadowRoot!.querySelector<ExtraContainersCreateDialog>(
              'settings-crostini-create-container-dialog');
      assertTrue(!!dialogElement);
      createDialogSubpage = dialogElement;

      const containerNameInputElement =
          createDialogSubpage.shadowRoot!.querySelector<CrInputElement>(
              '#containerNameInput');
      assertTrue(!!containerNameInputElement);
      containerNameInput = containerNameInputElement;

      const vmNameInputElement =
          createDialogSubpage.shadowRoot!.querySelector<CrInputElement>(
              '#vmNameInput');
      assertTrue(!!vmNameInputElement);
      vmNameInput = vmNameInputElement;

      const createButtonElement =
          createDialogSubpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#create');
      assertTrue(!!createButtonElement);
      createButton = createButtonElement;

      const advancedToggleElement =
          createDialogSubpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#advancedToggle');
      assertTrue(!!advancedToggleElement);
      advancedToggle = advancedToggleElement;

      const advancedSectionElement =
          createDialogSubpage.shadowRoot!.querySelector<HTMLElement>(
              '.advanced-section');
      assertTrue(!!advancedSectionElement);
      advancedSection = advancedSectionElement;
    });

    /**
     * Helper function to enter |inputValue| in the element |input| and fire an
     * input event.
     */
    function setInput(inputElement: CrInputElement, inputValue: string): void {
      inputElement.value = inputValue;
      inputElement.dispatchEvent(new Event('input'));
    }

    /**
     * Helper function to check that the containerNameInput is valid and
     * createButton is enabled.
     */
    function assertValidAndEnabled(): void {
      assertFalse(containerNameInput.invalid);
      assertFalse(createButton.disabled);
    }

    /**
     * Helper function to check that the containerNameInput is invalid with
     * |errorMsgName|, and createButton is disabled.
     */
    function assertInvalidAndDisabled(errorMsgName: string): void {
      assertTrue(containerNameInput.invalid);
      assertTrue(createButton.disabled);
      assertEquals(
          loadTimeData.getString(errorMsgName),
          containerNameInput.errorMessage);
    }

    test('AddContainerValidInDefaultVm', () => {
      setInput(containerNameInput, 'custom_container_2');
      assertValidAndEnabled();

      createButton.click();
      assertEquals(1, crostiniBrowserProxy.getCallCount('createContainer'));
    });

    test('AddContainerValidInNonDefaultVm', () => {
      setInput(containerNameInput, 'custom_container_1');
      setInput(vmNameInput, 'not_termina');
      assertValidAndEnabled();

      createButton.click();
      assertEquals(1, crostiniBrowserProxy.getCallCount('createContainer'));
    });

    test('ErrorAndDisabledCreateForDefaultContainerNameInDefaultVm', () => {
      setInput(containerNameInput, 'penguin');

      assertInvalidAndDisabled(
          'crostiniExtraContainersCreateDialogContainerExistsError');
    });

    test('ErrorAndDisabledCreateForDefaultContainerNameInNonDefaultVm', () => {
      setInput(containerNameInput, 'penguin');
      setInput(vmNameInput, 'not_termina');

      assertInvalidAndDisabled(
          'crostiniExtraContainersCreateDialogContainerExistsError');
    });

    test('ErrorAndDisabledCreateForDuplicateContainerNameInDefaultVm', () => {
      setInput(containerNameInput, 'custom_container_1');

      assertInvalidAndDisabled(
          'crostiniExtraContainersCreateDialogContainerExistsError');
    });

    test(
        'ErrorAndDisabledCreateForDuplicateContainerNameInNonDefaultVm', () => {
          setInput(containerNameInput, 'custom_container_2');
          setInput(vmNameInput, 'not_termina');

          assertInvalidAndDisabled(
              'crostiniExtraContainersCreateDialogContainerExistsError');
        });

    test('ErrorAndDisabledCreateForEmptyContainerNameInDefaultVm', () => {
      setInput(containerNameInput, '');

      assertInvalidAndDisabled(
          'crostiniExtraContainersCreateDialogEmptyContainerNameError');
    });

    test('ErrorAndDisabledCreateForEmptyContainerNameInNonDefaultVm', () => {
      setInput(containerNameInput, '');
      setInput(vmNameInput, 'not_termina');

      assertInvalidAndDisabled(
          'crostiniExtraContainersCreateDialogEmptyContainerNameError');
    });

    test('ReenabledButtonAfterError', () => {
      setInput(containerNameInput, 'penguin');
      assertInvalidAndDisabled(
          'crostiniExtraContainersCreateDialogContainerExistsError');

      setInput(containerNameInput, 'custom_container_2');
      assertValidAndEnabled();

      createButton.click();
      assertEquals(1, crostiniBrowserProxy.getCallCount('createContainer'));
    });

    test('CreateContainerAdvancedWithFile', () => {
      setInput(containerNameInput, 'advanced_container');
      setInput(vmNameInput, 'termina');

      assertTrue(advancedSection.hidden);
      advancedToggle.click();
      assertFalse(advancedSection.hidden);

      const containerFileInput =
          createDialogSubpage.shadowRoot!.querySelector<CrInputElement>(
              '#containerFileInput');
      assertTrue(!!containerFileInput);
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

  suite('ExportImportContainer', () => {
    test('Export', async () => {
      const showContainerMenuBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#showContainerMenu1');
      assertTrue(!!showContainerMenuBtn);
      showContainerMenuBtn.click();

      await flushTasks();
      const exportContainerBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#exportContainerButton');
      assertTrue(!!exportContainerBtn);
      exportContainerBtn.click();

      const args = crostiniBrowserProxy.getArgs('exportCrostiniContainer');
      assertEquals(1, args.length);
      assertEquals('termina', args[0].vm_name);
      assertEquals('custom_container_1', args[0].container_name);
    });

    test('Import', async () => {
      const showContainerMenuBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#showContainerMenu1');
      assertTrue(!!showContainerMenuBtn);
      showContainerMenuBtn.click();

      await flushTasks();
      const importContainerBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#importContainerButton');
      assertTrue(!!importContainerBtn);
      importContainerBtn.click();

      const args = crostiniBrowserProxy.getArgs('importCrostiniContainer');
      assertEquals(1, args.length);
      assertEquals('termina', args[0].vm_name);
      assertEquals('custom_container_1', args[0].container_name);
    });

    test('ExportImportButtonsGetDisabledOnOperationStatus', async () => {
      const showContainerMenuBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#showContainerMenu1');
      assertTrue(!!showContainerMenuBtn);
      showContainerMenuBtn.click();

      await flushTasks();
      let exportContainerBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#exportContainerButton');
      assertTrue(!!exportContainerBtn);
      let importContainerBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#importContainerButton');
      assertTrue(!!importContainerBtn);

      assertFalse(exportContainerBtn.disabled);
      assertFalse(importContainerBtn.disabled);
      webUIListenerCallback(
          'crostini-export-import-operation-status-changed', true);

      await flushTasks();
      exportContainerBtn =
          subpage.shadowRoot!.querySelector('#exportContainerButton');
      assertTrue(!!exportContainerBtn);
      importContainerBtn =
          subpage.shadowRoot!.querySelector('#importContainerButton');
      assertTrue(!!importContainerBtn);

      assertTrue(exportContainerBtn.disabled);
      assertTrue(importContainerBtn.disabled);
      webUIListenerCallback(
          'crostini-export-import-operation-status-changed', false);

      await flushTasks();
      exportContainerBtn =
          subpage.shadowRoot!.querySelector('#exportContainerButton');
      assertTrue(!!exportContainerBtn);
      importContainerBtn =
          subpage.shadowRoot!.querySelector('#importContainerButton');
      assertTrue(!!importContainerBtn);

      assertFalse(exportContainerBtn.disabled);
      assertFalse(importContainerBtn.disabled);
    });

    test('ExportImportButtonsDisabledOnWhenInstallingCrostini', async () => {
      const showContainerMenuBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#showContainerMenu1');
      assertTrue(!!showContainerMenuBtn);
      showContainerMenuBtn.click();

      await flushTasks();
      let exportContainerBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#exportContainerButton');
      assertTrue(!!exportContainerBtn);
      let importContainerBtn =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#importContainerButton');
      assertTrue(!!importContainerBtn);

      assertFalse(exportContainerBtn.disabled);
      assertFalse(importContainerBtn.disabled);
      webUIListenerCallback('crostini-installer-status-changed', true);

      await flushTasks();
      exportContainerBtn =
          subpage.shadowRoot!.querySelector('#exportContainerButton');
      assertTrue(!!exportContainerBtn);
      importContainerBtn =
          subpage.shadowRoot!.querySelector('#importContainerButton');
      assertTrue(!!importContainerBtn);

      assertTrue(exportContainerBtn.disabled);
      assertTrue(importContainerBtn.disabled);
      webUIListenerCallback('crostini-installer-status-changed', false);

      await flushTasks();
      exportContainerBtn =
          subpage.shadowRoot!.querySelector('#exportContainerButton');
      assertTrue(!!exportContainerBtn);
      importContainerBtn =
          subpage.shadowRoot!.querySelector('#importContainerButton');
      assertTrue(!!importContainerBtn);

      assertFalse(exportContainerBtn.disabled);
      assertFalse(importContainerBtn.disabled);
    });
  });

  suite('ContainerDetails', () => {
    test('ExpandButton', async () => {
      const expandButton = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#expand-button-termina-penguin');
      assertTrue(!!expandButton);

      // The collapse element should open/close on clicking |expandButton|.
      const collapse = subpage.shadowRoot!.querySelector<IronCollapseElement>(
          '#collapse-termina-penguin');
      assertTrue(!!collapse);

      assertFalse(collapse.opened);
      expandButton.click();
      await flushTasks();
      assertTrue(collapse.opened);

      expandButton.click();
      await flushTasks();
      assertFalse(collapse.opened);
    });

    test('ToggleMicrophoneOff', async () => {
      // The toggle is inside an iron-collapse, but we can still click it
      // via the testing apis.
      const toggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
          '#microphone-termina-penguin');

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
