// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feature-showcase/app.js';
import 'chrome://feature-showcase/password_manager/password_manager_step.js';

import type {FeatureShowcaseAppElement} from 'chrome://feature-showcase/app.js';
import {FeatureShowcasePageHandlerRemote} from 'chrome://feature-showcase/feature_showcase.mojom-webui.js';
import {FeatureShowcaseBrowserProxyImpl} from 'chrome://feature-showcase/feature_showcase_browser_proxy.js';
import {PasswordManagerPageHandlerRemote} from 'chrome://feature-showcase/password_manager.mojom-webui.js';
import {PasswordManagerBrowserProxyImpl} from 'chrome://feature-showcase/password_manager/password_manager_browser_proxy.js';
import type {FeatureShowcasePasswordManagerStepElement} from 'chrome://feature-showcase/password_manager/password_manager_step.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FeatureShowcaseAppTest', function() {
  let appElement: FeatureShowcaseAppElement;
  let testHandler: TestMock<FeatureShowcasePageHandlerRemote>&
      FeatureShowcasePageHandlerRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState({}, '', '?steps=example');

    testHandler = TestMock.fromClass(FeatureShowcasePageHandlerRemote);
    FeatureShowcaseBrowserProxyImpl.setInstance({handler: testHandler});

    appElement = document.createElement('feature-showcase-app');
    document.body.appendChild(appElement);
  });

  test('continue button clicked', async function() {
    await microtasksFinished();

    const exampleStep =
        appElement.shadowRoot.querySelector('feature-showcase-example-step');
    assertTrue(!!exampleStep);

    const button =
        exampleStep.shadowRoot.querySelector<HTMLElement>('#confirm-button');
    assertTrue(!!button);
    button.click();

    await testHandler.whenCalled('finishFeatureShowcase');
  });
});

suite('FeatureShowcasePasswordManagerStepTest', function() {
  let stepElement: FeatureShowcasePasswordManagerStepElement;
  let testHandler: TestMock<PasswordManagerPageHandlerRemote>&
      PasswordManagerPageHandlerRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testHandler = TestMock.fromClass(PasswordManagerPageHandlerRemote);
    PasswordManagerBrowserProxyImpl.setInstance({handler: testHandler});

    stepElement =
        document.createElement('feature-showcase-password-manager-step');
    document.body.appendChild(stepElement);
  });

  test('confirm button clicked', async function() {
    await microtasksFinished();

    const button =
        stepElement.shadowRoot.querySelector<HTMLElement>('#confirm-button');
    assertTrue(!!button);

    const stepCompletedEvent = new Promise((resolve) => {
      stepElement.addEventListener('step-completed', resolve);
    });

    button.click();

    await testHandler.whenCalled('pinPasswordManager');
    await stepCompletedEvent;
  });

  test('skip button clicked', async function() {
    await microtasksFinished();

    const button =
        stepElement.shadowRoot.querySelector<HTMLElement>('#skip-button');
    assertTrue(!!button);

    const stepCompletedEvent = new Promise((resolve) => {
      stepElement.addEventListener('step-completed', resolve);
    });

    button.click();

    await stepCompletedEvent;
    assertEquals(0, testHandler.getCallCount('pinPasswordManager'));
  });
});
