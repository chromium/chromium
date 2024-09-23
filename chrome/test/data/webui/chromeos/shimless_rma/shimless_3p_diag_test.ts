// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {Shimless3pDiagnostics} from 'chrome://shimless-rma/shimless_3p_diagnostics.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {Show3pDiagnosticsAppResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('shimless3pDiagTest', function() {
  let component: Shimless3pDiagnostics|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  // ShimlessRma is needed to handle the enable/disable button events.
  let shimlessRmaComponent: ShimlessRma|null = null;

  const providerName = 'Google';

  /**
   * Used to verify that all buttons has been disabled. This is to check if
   * users see the UI changes.
   * */
  let hasDisabledAllButtons = false;

  /**
   * The current disabled state of buttons.
   * */
  let isAllButtonsDisabled = false;

  const disableAllButtonsListener = () => {
    hasDisabledAllButtons = true;
    isAllButtonsDisabled = true;
  };

  const enableAllButtonsListener = () => {
    isAllButtonsDisabled = false;
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
    window.addEventListener('disable-all-buttons', disableAllButtonsListener);
    window.addEventListener('enable-all-buttons', enableAllButtonsListener);
    hasDisabledAllButtons = false;
    isAllButtonsDisabled = false;

    loadTimeData.overrideValues({'3pDiagnosticsEnabled': true});
    service.setGet3pDiagnosticsProviderResult(providerName);
    service.setInstallable3pDiagnosticsAppPath(null);
    service.setInstallLastFound3pDiagnosticsApp(null);
    service.setShow3pDiagnosticsAppResult(
        Show3pDiagnosticsAppResult.kAppNotInstalled);
  });

  teardown(() => {
    window.removeEventListener('enable-all-buttons', enableAllButtonsListener);
    window.removeEventListener(
        'disable-all-buttons', disableAllButtonsListener);
    component?.remove();
    component = null;
    shimlessRmaComponent?.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  function initialize(): Promise<void> {
    assert(!component);
    component = document.createElement(Shimless3pDiagnostics.is);
    assert(component);
    document.body.appendChild(component);

    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    return flushTasks();
  }

  function isDialogOpen(selector: string): boolean {
    assert(component);
    return strictQuery(selector, component.shadowRoot, CrDialogElement).open;
  }

  function clickButton(selector: string): Promise<void> {
    assert(component);
    strictQuery(selector, component.shadowRoot, CrButtonElement).click();
    return flushTasks();
  }

  function pressKey(
      key: string, altKey: boolean, shiftKey: boolean): Promise<void> {
    assert(component);
    const eventPromise = eventToPromise('keydown', component);
    component.dispatchEvent(new KeyboardEvent(
        'keydown',
        {
          bubbles: true,
          composed: true,
          key,
          altKey,
          shiftKey,
        },
        ));
    return eventPromise;
  }

  function pressEnterOnDialog(selector: string): Promise<void> {
    assert(component);
    const dialog = strictQuery(selector, component.shadowRoot, CrDialogElement);
    const eventPromise = eventToPromise('keypress', dialog);
    dialog.dispatchEvent(new KeyboardEvent(
        'keypress',
        {
          bubbles: true,
          composed: true,
          key: 'Enter',
        },
        ));
    return eventPromise;
  }

  function cancelDialog(selector: string): Promise<void> {
    assert(component);
    const dialog = strictQuery(selector, component.shadowRoot, CrDialogElement);
    const eventPromise = eventToPromise('cancel', component);
    dialog.getNative().dispatchEvent(
        new CustomEvent('cancel', {cancelable: true}));
    return eventPromise;
  }

  // Test initialization of 3p diag.
  test('initialize', async () => {
    await initialize();
    assert(component);
  });

  // Verify 3p diag is disabled by flag.
  test('3pDiagIsDisabledByFlag', async () => {
    loadTimeData.overrideValues({'3pDiagnosticsEnabled': false});
    await initialize();

    assert(component);
    component.launch3pDiagnostics();

    await flushTasks();
    assertFalse(hasDisabledAllButtons);
  });

  // If provider is not yet fetched, should not trigger the launch.
  test('ProviderIsNotYetFetched', async () => {
    // Set a delay to simulate the provider is not yet fetched. We don't
    // actually wait this to be done.
    service.setAsyncOperationDelayMs(1000);
    await initialize();

    assert(component);
    component.launch3pDiagnostics();

    await flushTasks();
    assertFalse(hasDisabledAllButtons);
  });

  // If no provider, should not trigger the launch.
  test('NoProvider', async () => {
    service.setGet3pDiagnosticsProviderResult(null);
    await initialize();

    assert(component);
    component.launch3pDiagnostics();

    await flushTasks();
    assertFalse(hasDisabledAllButtons);
  });

  // Verify the flow that users trigger 3p diag when there is no installed app.
  test('AppNotInstall', async () => {
    service.setShow3pDiagnosticsAppResult(
        Show3pDiagnosticsAppResult.kAppNotInstalled);
    await initialize();

    assert(component);
    component.launch3pDiagnostics();

    await flushTasks();
    assertTrue(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertEquals(
         'Google diagnostics app is not installed',
        strictQuery(
            '#shimless3pDiagErrorDialogTitle', component.shadowRoot,
            HTMLElement)
            .textContent!.trim());
    assertEquals(
        'Check with the device manufacturer',
        strictQuery(
            '#shimless3pDiagErrorDialogBody', component.shadowRoot, HTMLElement)
            .textContent!.trim());

    await clickButton('#shimless3pDiagErrorDialogButton');
    assertFalse(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertFalse(isAllButtonsDisabled);
  });

  // Verify the flow that users trigger 3p diag, there is an installed app, but
  // we fail to load the app.
  test('AppFailedToLoad', async () => {
    service.setShow3pDiagnosticsAppResult(
        Show3pDiagnosticsAppResult.kFailedToLoad);
    await initialize();

    assert(component);
    component.launch3pDiagnostics();

    await flushTasks();
    assertTrue(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertEquals(
        'Couldn\'t load Google diagnostics app',
        strictQuery(
            '#shimless3pDiagErrorDialogTitle', component.shadowRoot,
            HTMLElement)
            .textContent!.trim());
    assertEquals(
        'Try installing the app again',
        strictQuery(
            '#shimless3pDiagErrorDialogBody', component.shadowRoot, HTMLElement)
            .textContent!.trim());

    await clickButton('#shimless3pDiagErrorDialogButton');
    assertFalse(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertFalse(isAllButtonsDisabled);
  });

  // Test error dialog can be controlled by Enter and Escape (cancel event).
  for (const [name, actionOnDialog] of [
           ['Enter', pressEnterOnDialog],
           ['Cancel', cancelDialog],
  ]) {
    test(`ErrorDialogClosedBy${name}`, async () => {
      service.setShow3pDiagnosticsAppResult(
          Show3pDiagnosticsAppResult.kAppNotInstalled);
      await initialize();

      assert(component);
      component.launch3pDiagnostics();
      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagErrorDialog'));

      assert(actionOnDialog instanceof Function);
      await actionOnDialog('#shimless3pDiagErrorDialog');
      await flushTasks();
      assertFalse(isDialogOpen('#shimless3pDiagErrorDialog'));
      assertFalse(isAllButtonsDisabled);
    });
  }

  // Test wrong shortcut don't trigger 3p diag.
  for (const {name, key, altKey, shiftKey} of
           [{
             name: 'NoAltKey',
             key: 'D',
             altKey: false,
             shiftKey: true,
           },
            {
              name: 'NoShiftKey',
              key: 'D',
              altKey: true,
              shiftKey: false,
            },
            // No 'D' key
            {
              name: 'NotDKey',
              key: 'X',
              altKey: true,
              shiftKey: true,
            },
            // No 'D' key, 'L' is for logging dialog.
            {
              name: 'LKey',
              key: 'L',
              altKey: true,
              shiftKey: true,
            },
  ]) {
    test(`WrongShortcutDoesntTrigger3pDiag${name}`, async () => {
      await initialize();

      await pressKey(key, altKey, shiftKey);
      assertFalse(hasDisabledAllButtons);
    });
  }

  // Verify 3p diag flow by trigger keyboard shortcut.
  for (const {name, key, altKey, shiftKey} of
           [{
             name: 'UppercaseD',
             key: 'D',
             altKey: true,
             shiftKey: true,
           },
            {
              name: 'LowercaseD',
              key: 'd',
              altKey: true,
              shiftKey: true,
            },
  ]) {
    test(`Launch3pDiagByShortcut${name}`, async () => {
      service.setShow3pDiagnosticsAppResult(Show3pDiagnosticsAppResult.kOk);
      await initialize();

      await pressKey(key, altKey, shiftKey);
      await flushTasks();
      assertTrue(hasDisabledAllButtons);
      assertTrue(service.getWasShow3pDiagnosticsAppCalled());
      assertFalse(isAllButtonsDisabled);
    });
  }

  // Verify the flow that there is an installable app, users choose to skip the
  // installation, but there is no installed app.
  test('SkipInstallableButDontHaveInstalledApp', async () => {
    service.setInstallable3pDiagnosticsAppPath(
        {path: '/fake/installable.swbn'});
    service.setShow3pDiagnosticsAppResult(
        Show3pDiagnosticsAppResult.kAppNotInstalled);
    await initialize();

    assert(component);
    component.launch3pDiagnostics();

    await flushTasks();
    assertTrue(isDialogOpen('#shimless3pDiagFindInstallableDialog'));
    assertEquals(
        'Install Google diagnostics app?',
        strictQuery(
            '#shimless3pDiagFindInstallableDialogTitle', component.shadowRoot,
            HTMLElement)
            .textContent!.trim());
    assertEquals(
        'There is an installable app at /fake/installable.swbn',
        strictQuery(
            '#shimless3pDiagFindInstallableDialogBody', component.shadowRoot,
            HTMLElement)
            .textContent!.trim());

    await clickButton('#shimless3pDiagFindInstallableDialogSkipButton');
    await flushTasks();
    assertTrue(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertEquals(
        'Google diagnostics app is not installed',
        strictQuery(
            '#shimless3pDiagErrorDialogTitle', component.shadowRoot,
            HTMLElement)
            .textContent!.trim());
    assertEquals(
        'Check with the device manufacturer',
        strictQuery(
            '#shimless3pDiagErrorDialogBody', component.shadowRoot, HTMLElement)
            .textContent!.trim());

    await clickButton('#shimless3pDiagErrorDialogButton');
    assertFalse(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertFalse(isAllButtonsDisabled);
  });

  // Verify the flow that there is an installable app, users choose to skip the
  // installation, and there is an installed app.
  for (const [name, dialogAction] of [
           [
             'SkipInstallableByButton',
             () =>
                 clickButton('#shimless3pDiagFindInstallableDialogSkipButton'),
           ],
           [
             'SkipInstallableByCancel',
             () => cancelDialog('#shimless3pDiagFindInstallableDialog'),
           ],
  ]) {
    test(`${name}AndHasInstalledApp`, async () => {
      service.setInstallable3pDiagnosticsAppPath(
          {path: '/fake/installable.swbn'});
      service.setShow3pDiagnosticsAppResult(Show3pDiagnosticsAppResult.kOk);
      await initialize();

      assert(component);
      component.launch3pDiagnostics();

      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagFindInstallableDialog'));

      assert(dialogAction instanceof Function);
      await dialogAction();
      assertFalse(isDialogOpen('#shimless3pDiagFindInstallableDialog'));
      assertTrue(service.getWasShow3pDiagnosticsAppCalled());
      assertFalse(isAllButtonsDisabled);
    });
  }

  // Verify the flow that there is an installable app, users choose to install,
  // but fail to load the installable.
  for (const [name, dialogAction] of [
           [
             'InstallInstallableByButton',
             () => clickButton(
                 '#shimless3pDiagFindInstallableDialogInstallButton'),
           ],
           [
             'InstallInstallableByEnter',
             () => pressEnterOnDialog('#shimless3pDiagFindInstallableDialog'),
           ],
  ]) {
    test(`${name}ButFailToLoad`, async () => {
      service.setInstallable3pDiagnosticsAppPath(
          {path: '/fake/installable.swbn'});
      await initialize();

      assert(component);
      component.launch3pDiagnostics();

      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagFindInstallableDialog'));

      assert(dialogAction instanceof Function);
      await dialogAction();
      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagErrorDialog'));
      assertEquals(
          'Couldn\'t install Google diagnostics app',
          strictQuery(
              '#shimless3pDiagErrorDialogTitle', component.shadowRoot,
              HTMLElement)
              .textContent!.trim());
      assertEquals(
          'Check with the device manufacturer',
          strictQuery(
              '#shimless3pDiagErrorDialogBody', component.shadowRoot,
              HTMLElement)
              .textContent!.trim());

      await clickButton('#shimless3pDiagErrorDialogButton');
      assertFalse(isDialogOpen('#shimless3pDiagErrorDialog'));
      assertFalse(isAllButtonsDisabled);
    });
  }

  // Verify the flow that there is an installable app, users choose to install,
  // but don't accept the permission of the loaded app.
  for (const [name, dialogAction] of [
           [
             'DontAcceptByButton',
             () => clickButton(
                 '#shimless3pDiagReviewPermissionDialogCancelButton'),
           ],
           [
             'DontAcceptByCancel',
             () => cancelDialog('#shimless3pDiagReviewPermissionDialog'),
           ],
  ]) {
    test(`InstallInstallableBut${name}`, async () => {
      service.setInstallable3pDiagnosticsAppPath(
          {path: '/fake/installable.swbn'});
      service.setInstallLastFound3pDiagnosticsApp({
        name: 'Test Diag App',
        permissionMessage: 'Run diagnostics test\nGet device info\n',
      });
      await initialize();

      assert(component);
      component.launch3pDiagnostics();

      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagFindInstallableDialog'));

      await clickButton('#shimless3pDiagFindInstallableDialogInstallButton');
      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagReviewPermissionDialog'));
      assertEquals(
          'Review Test Diag App permissions',
          strictQuery(
              '#shimless3pDiagReviewPermissionDialogTitle',
              component.shadowRoot, HTMLElement)
              .textContent!.trim());
      assertDeepEquals(
          ['It can:', 'Run diagnostics test', 'Get device info', ''],
          strictQuery(
              '#shimless3pDiagReviewPermissionDialogMessage span',
              component.shadowRoot, HTMLElement)
              .textContent!.split('\n')
              .map(line => line.trim()));

      assert(dialogAction instanceof Function);
      await dialogAction();
      await flushTasks();

      assertFalse(
          service.getLastCompleteLast3pDiagnosticsInstallationApproval());
      assertFalse(isDialogOpen('#shimless3pDiagReviewPermissionDialog'));
      assertFalse(service.getWasShow3pDiagnosticsAppCalled());
      assertFalse(isAllButtonsDisabled);
    });
  }

  // Verify the flow that there is an installable app, users choose to install,
  // accept the permission of the loaded app, and the app is shown.
  for (const [name, dialogAction] of [
           [
             'AcceptByButton',
             () => clickButton(
                 '#shimless3pDiagReviewPermissionDialogAcceptButton'),
           ],
           [
             'AcceptByEnter',
             () => pressEnterOnDialog('#shimless3pDiagReviewPermissionDialog'),
           ],
  ]) {
    test(`InstallInstallableAnd${name}`, async () => {
      service.setInstallable3pDiagnosticsAppPath(
          {path: '/fake/installable.swbn'});
      service.setInstallLastFound3pDiagnosticsApp({
        name: 'Test Diag App',
        permissionMessage: 'Run diagnostics test\nGet device info\n',
      });
      service.setShow3pDiagnosticsAppResult(Show3pDiagnosticsAppResult.kOk);
      await initialize();

      assert(component);
      component.launch3pDiagnostics();

      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagFindInstallableDialog'));

      await clickButton('#shimless3pDiagFindInstallableDialogInstallButton');
      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagReviewPermissionDialog'));

      assert(dialogAction instanceof Function);
      await dialogAction();
      await flushTasks();

      assertTrue(
          service.getLastCompleteLast3pDiagnosticsInstallationApproval());
      assertFalse(isDialogOpen('#shimless3pDiagReviewPermissionDialog'));
      assertTrue(service.getWasShow3pDiagnosticsAppCalled());
      assertFalse(isAllButtonsDisabled);
    });
  }

  // Verify the flow that there is an installable app without permission
  // request, users choose to install, and the app is shown. If there is no
  // permission request the permission review step should be skipped.
  test('InstallInstallableNoPermissionRequest', async () => {
    service.setInstallable3pDiagnosticsAppPath(
        {path: '/fake/installable.swbn'});
    service.setInstallLastFound3pDiagnosticsApp({
      name: 'Test Diag App',
      permissionMessage: null,
    });
    service.setShow3pDiagnosticsAppResult(Show3pDiagnosticsAppResult.kOk);
    await initialize();

    assert(component);
    component.launch3pDiagnostics();

    await flushTasks();
    assertTrue(isDialogOpen('#shimless3pDiagFindInstallableDialog'));

    await clickButton('#shimless3pDiagFindInstallableDialogInstallButton');
    await flushTasks();
    const lastCompleteLast3pDiagnosticsInstallationApproval =
        service.getLastCompleteLast3pDiagnosticsInstallationApproval();
    assert(lastCompleteLast3pDiagnosticsInstallationApproval);
    assertTrue(service.getLastCompleteLast3pDiagnosticsInstallationApproval());
    assertTrue(service.getWasShow3pDiagnosticsAppCalled());
    assertFalse(isAllButtonsDisabled);
  });
});
