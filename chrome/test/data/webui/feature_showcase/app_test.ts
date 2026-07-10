// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feature-showcase/app.js';
import 'chrome://feature-showcase/feature_showcase_stepper.js';
import 'chrome://feature-showcase/password_manager/password_manager_step.js';
import 'chrome://feature-showcase/themes_and_customization/themes_and_customization_step.js';

import type {FeatureShowcaseAppElement} from 'chrome://feature-showcase/app.js';
import {browserProxyFactory as defaultBrowserProxyFactory, DefaultBrowserPageHandlerRemote} from 'chrome://feature-showcase/default_browser.mojom-webui.js';
import type {FeatureShowcaseDefaultBrowserStepElement} from 'chrome://feature-showcase/default_browser/default_browser_step.js';
import {FeatureShowcasePageHandlerRemote} from 'chrome://feature-showcase/feature_showcase.mojom-webui.js';
import {FeatureShowcaseBrowserProxyImpl} from 'chrome://feature-showcase/feature_showcase_browser_proxy.js';
import type {FeatureShowcaseStepperElement} from 'chrome://feature-showcase/feature_showcase_stepper.js';
import {PasswordManagerPageHandlerRemote} from 'chrome://feature-showcase/password_manager.mojom-webui.js';
import {PasswordManagerBrowserProxyImpl} from 'chrome://feature-showcase/password_manager/password_manager_browser_proxy.js';
import type {FeatureShowcasePasswordManagerStepElement} from 'chrome://feature-showcase/password_manager/password_manager_step.js';
import {ThemesAndCustomizationPageHandlerRemote} from 'chrome://feature-showcase/themes_and_customization.mojom-webui.js';
import {ThemesAndCustomizationBrowserProxyImpl} from 'chrome://feature-showcase/themes_and_customization/themes_and_customization_browser_proxy.js';
import type {FeatureShowcaseThemesAndCustomizationStepElement} from 'chrome://feature-showcase/themes_and_customization/themes_and_customization_step.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FeatureShowcaseAppTest', function() {
  let appElement: FeatureShowcaseAppElement;
  let testHandler: TestMock<FeatureShowcasePageHandlerRemote>&
      FeatureShowcasePageHandlerRemote;
  let originalMatchMedia: (query: string) => MediaQueryList;
  let mockMediaQueryList: EventTarget&{matches: boolean};

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState({}, '', '?steps=password-manager');

    const passwordManagerTestHandler:
        TestMock<PasswordManagerPageHandlerRemote>&
        PasswordManagerPageHandlerRemote =
        TestMock.fromClass(PasswordManagerPageHandlerRemote);
    PasswordManagerBrowserProxyImpl.setInstance(
        {handler: passwordManagerTestHandler});

    const defaultBrowserTestHandler: TestMock<DefaultBrowserPageHandlerRemote>&
        DefaultBrowserPageHandlerRemote =
        TestMock.fromClass(DefaultBrowserPageHandlerRemote);
    defaultBrowserProxyFactory.setInstance(
        {handler: defaultBrowserTestHandler});

    originalMatchMedia = window.matchMedia;
    mockMediaQueryList = new EventTarget() as EventTarget & {matches: boolean};
    mockMediaQueryList.matches = false;
    window.matchMedia = () => mockMediaQueryList as unknown as MediaQueryList;

    testHandler = TestMock.fromClass(FeatureShowcasePageHandlerRemote);
    FeatureShowcaseBrowserProxyImpl.setInstance({handler: testHandler});

    appElement = document.createElement('feature-showcase-app');
    document.body.appendChild(appElement);
  });

  teardown(function() {
    window.matchMedia = originalMatchMedia;
  });

  test(
      'finish feature showcase after only step continue button clicked',
      async function() {
        await microtasksFinished();

        const firstStep = appElement.shadowRoot.querySelector(
            'feature-showcase-password-manager-step');
        assertTrue(!!firstStep);

        const button =
            firstStep.shadowRoot.querySelector<HTMLElement>('#confirm-button');
        assertTrue(!!button);
        button.click();

        await testHandler.whenCalled('finishFeatureShowcase');
      });

  test('nextStepShown called on init', async function() {
    await testHandler.whenCalled('nextStepShown');
  });

  test('nextStepShown called on transition', async function() {
    // Setup app with 2 steps.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState(
        {}, '', '?steps=default-browser,password-manager');

    testHandler = TestMock.fromClass(FeatureShowcasePageHandlerRemote);
    FeatureShowcaseBrowserProxyImpl.setInstance({handler: testHandler});

    appElement = document.createElement('feature-showcase-app');
    document.body.appendChild(appElement);

    await testHandler.whenCalled('nextStepShown');

    testHandler.resetResolver('nextStepShown');

    const firstStep = appElement.shadowRoot.querySelector(
        'feature-showcase-default-browser-step');
    assertTrue(!!firstStep);

    const button =
        firstStep.shadowRoot.querySelector<HTMLElement>('#confirm-button');
    assertTrue(!!button);
    button.click();

    await testHandler.whenCalled('nextStepShown');
  });

  test('animation stays at correct frame on theme change', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState(
        {}, '', '?steps=default-browser,password-manager');
    appElement = document.createElement('feature-showcase-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const rightAnimation = appElement.$.rightAnimation;
    let rightSegments: [number, number]|null = null;
    rightAnimation.playSegments = (segments: [number, number]) => {
      rightSegments = segments;
    };

    const bottomAnimation = appElement.$.bottomAnimation;
    let bottomSegments: [number, number]|null = null;
    bottomAnimation.playSegments = (segments: [number, number]) => {
      bottomSegments = segments;
    };

    mockMediaQueryList.matches = true;
    mockMediaQueryList.dispatchEvent(new Event('change'));

    await microtasksFinished();

    assertDeepEquals([0, 1], rightSegments);
    assertDeepEquals([0, 1], bottomSegments);

    const firstStep = appElement.shadowRoot.querySelector(
        'feature-showcase-default-browser-step');
    firstStep!.dispatchEvent(new CustomEvent('step-completed'));
    await microtasksFinished();

    // Trigger another theme change
    mockMediaQueryList.matches = false;
    mockMediaQueryList.dispatchEvent(new Event('change'));
    await microtasksFinished();

    assertDeepEquals([120, 121], rightSegments);
    assertDeepEquals([120, 121], bottomSegments);
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

suite('FeatureShowcaseDefaultBrowserStepTest', function() {
  let stepElement: FeatureShowcaseDefaultBrowserStepElement;
  let testHandler: TestMock<DefaultBrowserPageHandlerRemote>&
      DefaultBrowserPageHandlerRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testHandler = TestMock.fromClass(DefaultBrowserPageHandlerRemote);
    defaultBrowserProxyFactory.setInstance({handler: testHandler});

    stepElement =
        document.createElement('feature-showcase-default-browser-step');
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

    await testHandler.whenCalled('setAsDefaultBrowser');
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

    await testHandler.whenCalled('skipSetAsDefaultBrowser');
    await stepCompletedEvent;
    assertEquals(0, testHandler.getCallCount('setAsDefaultBrowser'));
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

suite('FeatureShowcaseThemesAndCustomizationStepTest', function() {
  let stepElement: FeatureShowcaseThemesAndCustomizationStepElement;
  let testHandler: TestMock<ThemesAndCustomizationPageHandlerRemote>&
      ThemesAndCustomizationPageHandlerRemote;

  setup(function() {
    testHandler = TestMock.fromClass(ThemesAndCustomizationPageHandlerRemote);
    ThemesAndCustomizationBrowserProxyImpl.setInstance({handler: testHandler});

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    stepElement = document.createElement(
        'feature-showcase-themes-and-customization-step');
    document.body.appendChild(stepElement);
  });

  test('confirm button clicked', async function() {
    await microtasksFinished();

    await testHandler.whenCalled('snapshotTheme');

    const button =
        stepElement.shadowRoot.querySelector<HTMLElement>('#confirm-button');
    assertTrue(!!button);

    const stepCompletedEvent = new Promise((resolve) => {
      stepElement.addEventListener('step-completed', resolve);
    });

    button.click();

    await testHandler.whenCalled('acceptTheme');
    await stepCompletedEvent;
  });

  test('skip button clicked', async function() {
    await microtasksFinished();

    await testHandler.whenCalled('snapshotTheme');

    const button =
        stepElement.shadowRoot.querySelector<HTMLElement>('#skip-button');
    assertTrue(!!button);

    const stepCompletedEvent = new Promise((resolve) => {
      stepElement.addEventListener('step-completed', resolve);
    });

    button.click();

    await testHandler.whenCalled('revertTheme');
    await stepCompletedEvent;
    assertEquals(0, testHandler.getCallCount('acceptTheme'));
  });
});
