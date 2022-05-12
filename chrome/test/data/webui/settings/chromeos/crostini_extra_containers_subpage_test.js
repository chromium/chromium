// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrostiniBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

suite('CrostiniExtraContainersSubpageTests', function() {
  /** @type {?SettingsCrostiniPageElement} */
  let crostiniPage = null;

  /** @type {?TestCrostiniBrowserProxy} */
  let crostiniBrowserProxy = null;

  /** @type {?SettingsCrostiniSubPageElement} */
  let subpage;

  /** @type {?Element} */
  let createButton;

  setup(async function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.instance_ = crostiniBrowserProxy;
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();
    crostiniPage.prefs = {
      crostini: {
        enabled: {value: true},
      },
    };
    flush();
    Router.getInstance().navigateTo(
        routes.CROSTINI_EXTRA_CONTAINERS);

    await flushTasks();
    subpage = crostiniPage.$$('settings-crostini-extra-containers');
    assertTrue(!!subpage);

    subpage.allContainers_ = [
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

    setup(async function() {
      subpage.$$('#create').click();

      await flushTasks();
      subpage = subpage.$$('settings-crostini-create-container-dialog');

      containerNameInput = subpage.root.querySelector('#containerNameInput');
      vmNameInput = subpage.root.querySelector('#vmNameInput');
      createButton = subpage.root.querySelector('#create');
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
  });

  suite('ExportContainer', function() {
    test('Export', async function() {
      subpage.$$('#showContainerMenu1').click();

      await flushTasks();
      assertTrue(!!subpage.$$('#exportContainerButton'));
      subpage.$$('#exportContainerButton').click();
      assertEquals(
          1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
    });

    test('ExportImportButtonsGetDisabledOnOperationStatus', async function() {
      subpage.$$('#showContainerMenu1').click();

      await flushTasks();
      assertFalse(subpage.$$('#exportContainerButton').disabled);
      webUIListenerCallback(
          'crostini-export-import-operation-status-changed', true);

      await flushTasks();
      assertTrue(subpage.$$('#exportContainerButton').disabled);
      webUIListenerCallback(
          'crostini-export-import-operation-status-changed', false);

      await flushTasks();
      assertFalse(subpage.$$('#exportContainerButton').disabled);
    });

    test(
        'ExportImportButtonsDisabledOnWhenInstallingCrostini',
        async function() {
          subpage.$$('#showContainerMenu1').click();

          await flushTasks();
          assertFalse(subpage.$$('#exportContainerButton').disabled);
          webUIListenerCallback('crostini-installer-status-changed', true);

          await flushTasks();
          assertTrue(subpage.$$('#exportContainerButton').disabled);
          webUIListenerCallback('crostini-installer-status-changed', false);

          await flushTasks();
          assertFalse(subpage.$$('#exportContainerButton').disabled);
        });
  });
});
