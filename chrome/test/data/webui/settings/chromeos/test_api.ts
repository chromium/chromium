// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {LockScreenSettingsInterface, LockScreenSettingsReceiver, LockScreenSettingsRemote, OSSettingsBrowserProcess, OSSettingsDriverInterface, OSSettingsDriverReceiver} from './test_api.test-mojom-webui.js';

const kDefaultTimeoutMilliseconds = 10 * 1000;

// Applies a function to an element until the function returns not null.
// Reapplies the function if the element changes.
async function watchElement<E extends DocumentFragment|Element, T>(
    element: E, elementFunction: (el: E) => (T | null),
    timeoutMilliseconds: number =
        kDefaultTimeoutMilliseconds): Promise<T|null> {
  const result = elementFunction(element);
  if (result != null) {
    return result;
  }

  const asyncResult = await new Promise((resolve: (val: T|null) => void) => {
    interface State {
      mutationObserver: MutationObserver|null;
      timeoutID: number|null;
    }
    const state: State = {
      mutationObserver: null,
      timeoutID: null,
    };

    const done = (result: T|null) => {
      if (state.mutationObserver != null) {
        state.mutationObserver.disconnect();
      }
      if (state.timeoutID != null) {
        clearTimeout(state.timeoutID);
      }
      resolve(result);
    };

    state.mutationObserver = new MutationObserver(() => {
      const result = elementFunction(element);
      if (result != null) {
        done(result);
      }
    });
    state.mutationObserver.observe(
        element, {childList: true, subtree: true, attributes: true});

    state.timeoutID = setTimeout(() => {
      done(null);
    }, timeoutMilliseconds);
  });

  return asyncResult;
}

// Like querySelector, but waits for the element to appear if necessary.
async function querySelectorAsync(
    root: DocumentFragment|Element, selector: string,
    timeoutMilliseconds: number =
        kDefaultTimeoutMilliseconds): Promise<Element|null> {
  return watchElement(
      root, (root) => root.querySelector(selector), timeoutMilliseconds);
}

// Find an element that is nested inside shadow roots using a sequence of query
// selectors. The first query is run from |root|. Subsequent queries are run
// within the |shadowRoot| of the previous result. Returns |null| if any of
// the queries did not yield a result.
async function querySelectorAsyncShadowRoots(
    root: DocumentFragment|Element, selectors: string[],
    timeoutMilliseconds: number =
        kDefaultTimeoutMilliseconds): Promise<Element|null> {
  assertTrue(selectors.length > 0);

  const initSelectors = selectors.slice(0, selectors.length - 1);
  const lastSelector = selectors[selectors.length - 1];
  assertTrue(lastSelector != null);
  for (const selector of initSelectors) {
    const el = await querySelectorAsync(root, selector, timeoutMilliseconds);
    if (el == null) {
      return null;
    }
    const shadowRoot =
        await watchElement(el, (el) => el.shadowRoot, timeoutMilliseconds);
    if (shadowRoot == null) {
      return null;
    }
    root = shadowRoot;
  }

  return querySelectorAsync(root, lastSelector, timeoutMilliseconds);
}

export class LockScreenSettings implements LockScreenSettingsInterface {
  privacyPage: Element;
  lockScreen: Element;

  constructor(privacyPage: Element) {
    this.privacyPage = privacyPage;
    assertTrue(privacyPage.shadowRoot != null);
    const lockScreen =
        privacyPage.shadowRoot.querySelector('settings-lock-screen');
    assertTrue(lockScreen != null);
    this.lockScreen = lockScreen;
  }

  async passwordPrompt(): Promise<Element|null> {
    assertTrue(this.privacyPage.shadowRoot != null);
    const passwordDialog = await querySelectorAsync(
        this.privacyPage.shadowRoot, '#passwordDialog');
    if (passwordDialog == null) {
      return null;
    }

    assertTrue(passwordDialog.shadowRoot != null);
    return querySelectorAsync(passwordDialog.shadowRoot, '#passwordPrompt');
  }

  async authenticate(password: string): Promise<void> {
    const passwordPrompt = await this.passwordPrompt();
    assertTrue(passwordPrompt != null);
    assertTrue(passwordPrompt.shadowRoot != null);

    const passwordInput =
        await querySelectorAsync(passwordPrompt.shadowRoot, '#passwordInput');
    assertTrue(passwordInput != null);
    (passwordInput as HTMLElement & {value: any}).value = password;

    const confirmButton =
        await querySelectorAsync(passwordPrompt.shadowRoot, '#confirmButton');
    assertTrue(confirmButton instanceof HTMLElement);
    confirmButton.click();
  }

  async recoveryToggle(): Promise<HTMLElement|null> {
    assertTrue(this.lockScreen.shadowRoot != null);
    const toggle =
        await querySelectorAsync(this.lockScreen.shadowRoot, '#recoveryToggle');
    assertTrue(toggle == null || toggle instanceof HTMLElement);
    return toggle;
  }

  async assertRecoveryControlVisibility(isVisible: boolean): Promise<void> {
    // TODO(b/239416325): We could be more thorough here -- check that the
    // toggle does not have visibility: hidden, has non-zero size, is not
    // covered by other elements etc.
    const toggle = await this.recoveryToggle();
    const toggleExists = toggle != null;
    assertTrue(isVisible === toggleExists);
  }

  async assertRecoveryConfigured(isConfigured: boolean): Promise<void> {
    const toggle =
        await this.recoveryToggle() as HTMLElement & {checked: boolean};
    assertTrue(toggle.checked === isConfigured);
  }

  async toggleRecoveryConfiguration(): Promise<void> {
    const toggle = await this.recoveryToggle() as HTMLElement &
        {checked: boolean, disabled: boolean};
    assertTrue(toggle != null);

    const previouslyChecked = toggle.checked;
    toggle.click();

    // If the toggle is immediately toggled, that's OK. Otherwise we wait wait
    // until it becomes toggled.
    if (toggle.checked === !previouslyChecked) {
      return;
    }

    assertTrue(toggle.disabled);
    // Click again to see whether something weird happens.
    toggle.click();

    // Check that toggle is eventually checked.
    const notNullIfToggled = await watchElement(toggle, (toggle) => {
      if (toggle.checked === !previouslyChecked) {
        return {};
      }
      return null;
    });

    assertTrue(notNullIfToggled != null);
  }
}

class OSSettingsDriver implements OSSettingsDriverInterface {
  async goToLockScreenSettings():
      Promise<{lockScreenSettings: LockScreenSettingsRemote}> {
    const privacyPage = await querySelectorAsyncShadowRoots(document.body, [
      'os-settings-ui',
      'os-settings-main',
      'os-settings-page',
      'os-settings-privacy-page',
    ]);
    assertTrue(privacyPage != null);
    assertTrue(privacyPage.shadowRoot != null);

    const trigger = await querySelectorAsync(
        privacyPage.shadowRoot, '#lockScreenSubpageTrigger');
    assertTrue(trigger instanceof HTMLElement);
    trigger.click();

    const lockScreen = new LockScreenSettings(privacyPage);
    const receiver = new LockScreenSettingsReceiver(lockScreen);
    const remote = receiver.$.bindNewPipeAndPassRemote();

    return {lockScreenSettings: remote};
  }
}

// Passes an OSSettingsDriver remote to the browser process.
export async function register(): Promise<void> {
  const browserProcess = OSSettingsBrowserProcess.getRemote();
  const receiver = new OSSettingsDriverReceiver(new OSSettingsDriver());
  const remote = receiver.$.bindNewPipeAndPassRemote();
  await browserProcess.registerOSSettingsDriver(remote);
}
