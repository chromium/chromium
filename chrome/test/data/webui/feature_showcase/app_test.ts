// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feature-showcase/app.js';
import 'chrome://feature-showcase/feature_showcase_stepper.js';
import 'chrome://feature-showcase/password_manager/password_manager_step.js';

import type {FeatureShowcaseAppElement} from 'chrome://feature-showcase/app.js';
import {FeatureShowcasePageHandlerRemote} from 'chrome://feature-showcase/feature_showcase.mojom-webui.js';
import {FeatureShowcaseBrowserProxyImpl} from 'chrome://feature-showcase/feature_showcase_browser_proxy.js';
import type {FeatureShowcaseStepperElement} from 'chrome://feature-showcase/feature_showcase_stepper.js';
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

suite('FeatureShowcaseStepperTest', function() {
  let stepperElement: FeatureShowcaseStepperElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    stepperElement = document.createElement('feature-showcase-stepper');
    document.body.appendChild(stepperElement);
  });

  test('renders single ball for less than 3 steps', async function() {
    stepperElement.steps = ['step1', 'step2'];
    stepperElement.activeIndex = 0;
    await microtasksFinished();

    const steps = stepperElement.shadowRoot.querySelectorAll('.step');
    assertEquals(1, steps.length);
    const img = steps[0]?.querySelector('img');
    assertTrue(!!img);
    assertTrue(img.src.includes('product-logo.svg'));
  });

  test('renders stepper for 3 or more steps', async function() {
    stepperElement.steps = ['step1', 'step2', 'step3'];
    stepperElement.activeIndex = 1;  // 2nd step
    await microtasksFinished();

    const steps = stepperElement.shadowRoot.querySelectorAll('.step');
    assertEquals(3, steps.length);

    // Completed step
    const icon_completed = steps[0]?.querySelector('cr-icon');
    assertTrue(!!icon_completed);
    assertEquals('cr:check', icon_completed.icon);

    // Current step
    const img_active = steps[1]?.querySelector('img');
    assertTrue(!!img_active);
    assertTrue(img_active.src.includes('product-logo.svg'));

    // Upcoming step
    const dot_upcoming = steps[2]?.querySelector('.dot');
    assertTrue(!!dot_upcoming);
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
