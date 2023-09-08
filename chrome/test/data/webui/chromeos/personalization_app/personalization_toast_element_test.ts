// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {DismissErrorAction, PersonalizationActionName, PersonalizationToastElement} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('PersonalizationToastTest', function() {
  let personalizationToastElement: PersonalizationToastElement;

  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationToastElement = initElement(PersonalizationToastElement);
  });

  teardown(async () => {
    await teardownElement(personalizationToastElement);
    await flushTasks();
  });

  test('hidden when no error is present', async () => {
    assertEquals('', personalizationToastElement.innerHTML);
  });

  [true, false].forEach(
      (overrideDismissMessage:
           boolean) => test('visible when error is present', async () => {
        personalizationStore.data.error = {message: 'There was an error'};
        if (overrideDismissMessage) {
          personalizationStore.data.error.dismiss = {message: 'Overridden'};
        }
        personalizationStore.notifyObservers();
        await waitAfterNextRender(personalizationToastElement);
        assertTrue(!!personalizationToastElement.shadowRoot!.getElementById(
            'container'));
        assertEquals(
            personalizationStore.data.error.message,
            personalizationToastElement.shadowRoot!.querySelector(
                                                       'p')!.innerText);
        assertEquals(
            overrideDismissMessage ? 'Overridden' : 'Dismiss',
            personalizationToastElement.shadowRoot!.querySelector(
                                                       'cr-button')!.innerText);
      }));

  test('dispatches a dismiss action when dismiss is clicked', async () => {
    personalizationStore.data.error = {message: 'There was an error'};
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationToastElement);

    personalizationStore.expectAction(PersonalizationActionName.DISMISS_ERROR);
    personalizationToastElement.shadowRoot!.querySelector('cr-button')!.click();
    const dismissErrorAction =
        await personalizationStore.waitForAction(
            PersonalizationActionName.DISMISS_ERROR) as DismissErrorAction;
    assertEquals(dismissErrorAction.fromUser, true);
  });

  test('invokes callback when dismiss is clicked', async () => {
    let dismissCallback: ((byUser: boolean) => void)|undefined = undefined;
    const dismissCallbackPromise = new Promise<boolean>(resolve => {
      dismissCallback = resolve;
    });

    personalizationStore.data.error = {
      message: 'There was an error',
      dismiss: {callback: dismissCallback},
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationToastElement);

    personalizationStore.setReducersEnabled(true);
    personalizationToastElement.shadowRoot!.querySelector('cr-button')!.click();
    assertEquals(await dismissCallbackPromise, /*fromUser=*/ true);
  });

  test('automatically dismisses after ten seconds', async () => {
    // Spy on calls to |window.setTimeout|.
    const setTimeout = window.setTimeout;
    const setTimeoutCalls: Array<{handler: Function | string, delay?: number}> =
        [];
    window.setTimeout =
        (handler: Function|string, delay?: number, ...args: any[]): number => {
          setTimeoutCalls.push({handler, delay});
          return setTimeout(handler, delay, args);
        };

    // Create and render an error.
    personalizationStore.data.error = {message: 'There was an error.'};
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationToastElement);

    // Expect that a timeout will have been scheduled for 10 seconds.
    const setTimeoutCall: {handler: Function|string, delay?: number}|undefined =
        setTimeoutCalls.find((setTimeoutCall) => {
          return typeof setTimeoutCall.handler === 'function' &&
              setTimeoutCall.delay === 10000;
        });
    assertNotEquals(setTimeoutCall, undefined);

    // Expect that the timeout will result in error dismissal.
    personalizationStore.expectAction(PersonalizationActionName.DISMISS_ERROR);
    (setTimeoutCall!.handler as Function)();
    const dismissErrorAction =
        await personalizationStore.waitForAction(
            PersonalizationActionName.DISMISS_ERROR) as DismissErrorAction;
    assertEquals(dismissErrorAction.fromUser, false);
  });
});
