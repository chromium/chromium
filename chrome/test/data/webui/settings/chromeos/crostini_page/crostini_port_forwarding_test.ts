// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ContainerInfo, ContainerSelectElement, CrostiniBrowserProxyImpl, CrostiniPortForwardingElement, CrostiniPortSetting} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement, CrToastElement, CrToggleElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

let subpage: CrostiniPortForwardingElement;
let crostiniBrowserProxy: TestCrostiniBrowserProxy;

interface PrefParams {
  sharedPaths?: {[key: string]: string[]};
  forwardedPorts?: CrostiniPortSetting[];
  micAllowed?: boolean;
  arcEnabled?: boolean;
  bruschettaInstalled?: boolean;
}

function setCrostiniPrefs(enabled: boolean, {
  sharedPaths = {},
  forwardedPorts = [],
  micAllowed = false,
  arcEnabled = false,
  bruschettaInstalled = false,
}: PrefParams = {}): void {
  subpage.prefs = {
    arc: {
      enabled: {value: arcEnabled},
    },
    bruschetta: {
      installed: {
        value: bruschettaInstalled,
      },
    },
    crostini: {
      enabled: {value: enabled},
      mic_allowed: {value: micAllowed},
      port_forwarding: {ports: {value: forwardedPorts}},
    },
    guest_os: {
      paths_shared_to_vms: {value: sharedPaths},
    },
  };
  flush();
}

function selectContainerByIndex(
    select: ContainerSelectElement, index: number): void {
  const mdSelect = select.shadowRoot!.querySelector<HTMLSelectElement>(
      'select#selectContainer.md-select');
  assertTrue(!!mdSelect);
  mdSelect.selectedIndex = index;
  mdSelect.dispatchEvent(new CustomEvent('change'));
  flush();
}

const allContainers: ContainerInfo[] = [
  {
    id: {
      vm_name: 'termina',
      container_name: 'penguin',
    },
    ipv4: '1.2.3.4',
  },
  {
    id: {
      vm_name: 'not-termina',
      container_name: 'not-penguin',

    },
    ipv4: '1.2.3.5',
  },
];

suite('<settings-crostini-port-forwarding>', () => {
  setup(async () => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
    });

    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    crostiniBrowserProxy.portOperationSuccess = true;
    crostiniBrowserProxy.containerInfo = allContainers;
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);

    Router.getInstance().navigateTo(routes.CROSTINI_PORT_FORWARDING);
    subpage = document.createElement('settings-crostini-port-forwarding');
    document.body.appendChild(subpage);
    setCrostiniPrefs(true, {
      forwardedPorts: [
        {
          port_number: 5000,
          protocol_type: 0,
          label: 'Label1',
          vm_name: 'termina',
          container_name: 'penguin',
          container_id: {
            vm_name: 'termina',
            container_name: 'penguin',
          },
          is_active: false,
        },
        {
          port_number: 5001,
          protocol_type: 1,
          label: 'Label2',
          vm_name: 'not-termina',
          container_name: 'not-penguin',
          container_id: {
            vm_name: 'not-termina',
            container_name: 'not-penguin',
          },
          is_active: false,
        },
      ],
    });
    await flushTasks();

    assertEquals(1, crostiniBrowserProxy.getCallCount('requestContainerInfo'));
  });

  teardown(() => {
    subpage.remove();
    Router.getInstance().resetRouteForTesting();
    crostiniBrowserProxy.reset();
  });

  test('Display ports', () => {
    // Extra list item for the titles.
    assertEquals(4, subpage.shadowRoot!.querySelectorAll('.list-item').length);
  });

  test('Add port success', async () => {
    const addPortBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#addPort cr-button');
    assertTrue(!!addPortBtn);
    addPortBtn.click();

    await flushTasks();
    const addPortDialogElement =
        subpage.shadowRoot!.querySelector('settings-crostini-add-port-dialog');
    assertTrue(!!addPortDialogElement);
    const portNumberInput =
        addPortDialogElement.shadowRoot!.querySelector<CrInputElement>(
            '#portNumberInput');
    assertTrue(!!portNumberInput);
    portNumberInput.focus();
    portNumberInput.value = '5002';
    portNumberInput.blur();
    assertFalse(portNumberInput.invalid);
    const portLabelInput =
        addPortDialogElement.shadowRoot!.querySelector<CrInputElement>(
            '#portLabelInput');
    assertTrue(!!portLabelInput);
    portLabelInput.value = 'Some Label';
    const select = addPortDialogElement.shadowRoot!.querySelector(
        'settings-guest-os-container-select');
    assertTrue(!!select);
    selectContainerByIndex(select, 1);

    const continueButton =
        addPortDialogElement.shadowRoot!.querySelector<HTMLButtonElement>(
            'cr-dialog cr-button[id="continue"]');
    assertTrue(!!continueButton);
    continueButton.click();
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
    const args = crostiniBrowserProxy.getArgs('addCrostiniPortForward')[0];
    assertEquals(4, args.length);
    assertEquals('not-termina', args[0].vm_name);
    assertEquals('not-penguin', args[0].container_name);
  });

  test('Add port fail', async () => {
    const addPortBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#addPort cr-button');
    assertTrue(!!addPortBtn);
    addPortBtn.click();

    await flushTasks();
    const addPortDialogElement =
        subpage.shadowRoot!.querySelector('settings-crostini-add-port-dialog');
    assertTrue(!!addPortDialogElement);
    const portNumberInput =
        addPortDialogElement.shadowRoot!.querySelector<CrInputElement>(
            '#portNumberInput');
    assertTrue(!!portNumberInput);
    const continueButton =
        addPortDialogElement.shadowRoot!.querySelector<HTMLButtonElement>(
            'cr-dialog cr-button[id="continue"]');
    assertTrue(!!continueButton);

    assertFalse(portNumberInput.invalid);
    portNumberInput.focus();
    portNumberInput.value = '1023';
    continueButton.click();
    assertEquals(
        0, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
    assertTrue(continueButton.disabled);
    assertTrue(portNumberInput.invalid);
    assertEquals(
        loadTimeData.getString('crostiniPortForwardingAddError'),
        portNumberInput.errorMessage);

    portNumberInput.value = '65536';
    assertTrue(continueButton.disabled);
    assertTrue(portNumberInput.invalid);
    assertEquals(
        loadTimeData.getString('crostiniPortForwardingAddError'),
        portNumberInput.errorMessage);

    portNumberInput.focus();
    portNumberInput.value = '5000';
    portNumberInput.blur();

    continueButton.click();
    assertTrue(continueButton.disabled);
    assertTrue(portNumberInput.invalid);
    assertEquals(
        loadTimeData.getString('crostiniPortForwardingAddExisting'),
        portNumberInput.errorMessage);

    portNumberInput.focus();
    portNumberInput.value = '1024';
    portNumberInput.blur();
    assertFalse(continueButton.disabled);
    assertFalse(portNumberInput.invalid);
    assertEquals('', portNumberInput.errorMessage);
  });

  test('Add port cancel', async () => {
    const addPortBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#addPort cr-button');
    assertTrue(!!addPortBtn);
    addPortBtn.click();

    await flushTasks();
    const addPortDialogElement =
        subpage.shadowRoot!.querySelector('settings-crostini-add-port-dialog');
    assertTrue(!!addPortDialogElement);
    const cancelBtn =
        addPortDialogElement.shadowRoot!.querySelector<HTMLButtonElement>(
            'cr-dialog cr-button[id="cancel"]');
    assertTrue(!!cancelBtn);
    cancelBtn.click();
    flush();

    assertFalse(isVisible(addPortDialogElement));
  });

  test('Remove all ports', async () => {
    const showMenuBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#showRemoveAllPortsMenu');
    assertTrue(!!showMenuBtn);
    showMenuBtn.click();

    await flushTasks();
    const removeBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#removeAllPortsButton');
    assertTrue(!!removeBtn);
    removeBtn.click();
    assertEquals(
        2, crostiniBrowserProxy.getCallCount('removeAllCrostiniPortForwards'));
  });

  test('Remove single port', () => {
    const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#removeSinglePortButton0-0');
    assertTrue(!!button);
    button.click();
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('removeCrostiniPortForward'));
    const args = crostiniBrowserProxy.getArgs('removeCrostiniPortForward')[0];
    assertEquals(3, args.length);
    assertEquals('termina', args[0].vm_name);
    assertEquals('penguin', args[0].container_name);
  });

  test('Activate single port success', async () => {
    let errorToast =
        subpage.shadowRoot!.querySelector<CrToastElement>('#errorToast');
    assertTrue(!!errorToast);
    assertFalse(errorToast.open);
    await flushTasks();

    const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
        '#toggleActivationButton0-0');
    assertTrue(!!crToggle);
    assertFalse(crToggle.disabled);
    crToggle.click();

    await flushTasks();
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
    errorToast =
        subpage.shadowRoot!.querySelector<CrToastElement>('#errorToast');
    assertTrue(!!errorToast);
    assertFalse(errorToast.open);
  });

  test('Activate single port fail', async () => {
    crostiniBrowserProxy.portOperationSuccess = false;
    let errorToast =
        subpage.shadowRoot!.querySelector<CrToastElement>('#errorToast');
    assertTrue(!!errorToast);
    assertFalse(errorToast.open);

    const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
        '#toggleActivationButton1-0');
    assertTrue(!!crToggle);
    assertFalse(crToggle.disabled);
    assertFalse(crToggle.checked);
    crToggle.click();

    await flushTasks();
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
    assertFalse(crToggle.checked);
    errorToast =
        subpage.shadowRoot!.querySelector<CrToastElement>('#errorToast');
    assertTrue(!!errorToast);
    assertTrue(errorToast.open);
  });

  test('Deactivate single port', async () => {
    const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
        '#toggleActivationButton0-0');
    assertTrue(!!crToggle);
    assertFalse(crToggle.disabled);
    crToggle.checked = true;
    crToggle.click();

    await flushTasks();
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('deactivateCrostiniPortForward'));
  });

  test('Active ports changed', async () => {
    setCrostiniPrefs(true, {
      forwardedPorts: [
        {
          port_number: 5000,
          protocol_type: 0,
          label: 'Label1',
          vm_name: 'termina',
          container_name: 'penguin',
          container_id: {
            vm_name: 'termina',
            container_name: 'penguin',
          },
          is_active: false,
        },
      ],
    });
    const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
        '#toggleActivationButton0-0');
    assertTrue(!!crToggle);

    webUIListenerCallback(
        'crostini-port-forwarder-active-ports-changed',
        [{'port_number': 5000, 'protocol_type': 0}]);
    await flushTasks();
    assertTrue(crToggle.checked);

    webUIListenerCallback('crostini-port-forwarder-active-ports-changed', []);
    await flushTasks();
    assertFalse(crToggle.checked);
  });

  test('Port prefs change', () => {
    // Default prefs should have list items per port, plus one per
    // container.
    assertEquals(4, subpage.shadowRoot!.querySelectorAll('.list-item').length);

    // When only one of the default container has ports, we lose an item for
    // the extra container heading.
    setCrostiniPrefs(true, {
      forwardedPorts: [
        {
          port_number: 5000,
          protocol_type: 0,
          label: 'Label1',
          vm_name: 'termina',
          container_name: 'penguin',
          container_id: {
            vm_name: 'termina',
            container_name: 'penguin',
          },
          is_active: false,
        },
        {
          port_number: 5001,
          protocol_type: 0,
          label: 'Label2',
          vm_name: 'termina',
          container_name: 'penguin',
          container_id: {
            vm_name: 'termina',
            container_name: 'penguin',
          },
          is_active: false,
        },
      ],
    });
    assertEquals(3, subpage.shadowRoot!.querySelectorAll('.list-item').length);
    setCrostiniPrefs(true, {
      forwardedPorts: [
        {
          port_number: 5000,
          protocol_type: 0,
          label: 'Label1',
          vm_name: 'termina',
          container_name: 'penguin',
          container_id: {
            vm_name: 'termina',
            container_name: 'penguin',
          },
          is_active: false,
        },
        {
          port_number: 5001,
          protocol_type: 0,
          label: 'Label2',
          vm_name: 'termina',
          container_name: 'penguin',
          container_id: {
            vm_name: 'termina',
            container_name: 'penguin',
          },
          is_active: false,
        },
        {
          port_number: 5002,
          protocol_type: 0,
          label: 'Label3',
          vm_name: 'termina',
          container_name: 'penguin',
          container_id: {
            vm_name: 'termina',
            container_name: 'penguin',
          },
          is_active: false,
        },
      ],
    });
    assertEquals(4, subpage.shadowRoot!.querySelectorAll('.list-item').length);
    setCrostiniPrefs(true, {forwardedPorts: []});
    assertEquals(0, subpage.shadowRoot!.querySelectorAll('.list-item').length);
  });

  test('Container stop and start', async () => {
    const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
        '#toggleActivationButton0-0');
    assertTrue(!!crToggle);
    assertFalse(crToggle.disabled);

    allContainers[0]!.ipv4 = null;
    webUIListenerCallback(
        'crostini-container-info', structuredClone(allContainers));
    await flushTasks();
    assertTrue(crToggle.disabled);

    allContainers[0]!.ipv4 = '1.2.3.4';
    webUIListenerCallback(
        'crostini-container-info', structuredClone(allContainers));
    await flushTasks();
    assertFalse(crToggle.disabled);
  });
});
