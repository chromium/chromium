// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feature-showcase/app.js';
import 'chrome://feature-showcase/feature_showcase_stepper.js';
import 'chrome://feature-showcase/gemini/gemini_step.js';
import 'chrome://feature-showcase/password_manager/password_manager_step.js';
import 'chrome://feature-showcase/themes_and_customization/themes_and_customization_step.js';

import type {FeatureShowcaseAppElement} from 'chrome://feature-showcase/app.js';
import {browserProxyFactory as defaultBrowserProxyFactory, DefaultBrowserPageHandlerRemote} from 'chrome://feature-showcase/default_browser.mojom-webui.js';
import type {FeatureShowcaseDefaultBrowserStepElement} from 'chrome://feature-showcase/default_browser/default_browser_step.js';
import {browserProxyFactory as featureShowcaseProxyFactory, FeatureShowcasePageHandlerRemote} from 'chrome://feature-showcase/feature_showcase.mojom-webui.js';
import type {FeatureShowcaseStepperElement} from 'chrome://feature-showcase/feature_showcase_stepper.js';
import {browserProxyFactory as geminiProxyFactory, GeminiPageHandlerRemote} from 'chrome://feature-showcase/gemini.mojom-webui.js';
import type {FeatureShowcaseGeminiStepElement} from 'chrome://feature-showcase/gemini/gemini_step.js';
import {browserProxyFactory as passwordManagerProxyFactory, PasswordManagerPageHandlerRemote} from 'chrome://feature-showcase/password_manager.mojom-webui.js';
import type {FeatureShowcasePasswordManagerStepElement} from 'chrome://feature-showcase/password_manager/password_manager_step.js';
import {browserProxyFactory as themesAndCustomizationProxyFactory, ThemesAndCustomizationPageHandlerRemote} from 'chrome://feature-showcase/themes_and_customization.mojom-webui.js';
import type {FeatureShowcaseThemesAndCustomizationStepElement} from 'chrome://feature-showcase/themes_and_customization/themes_and_customization_step.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';
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
    passwordManagerProxyFactory.setInstance(
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
    featureShowcaseProxyFactory.setInstance({handler: testHandler});

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
    featureShowcaseProxyFactory.setInstance({handler: testHandler});

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
    testHandler.resetResolver('nextStepShown');
    firstStep!.dispatchEvent(new CustomEvent('step-completed'));
    await testHandler.whenCalled('nextStepShown');

    // Trigger another theme change
    mockMediaQueryList.matches = false;
    mockMediaQueryList.dispatchEvent(new Event('change'));
    await microtasksFinished();

    assertDeepEquals([120, 121], rightSegments);
    assertDeepEquals([120, 121], bottomSegments);
  });

  test(
      'waits for last stepper animation before finishing feature showcase',
      async function() {
        await microtasksFinished();
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        testHandler.reset();

        const url = new URL(window.location.href);
        url.searchParams.set(
            'steps',
            'default-browser,themes-and-customization,password-manager');
        window.history.replaceState({}, '', url.toString());

        appElement = document.createElement('feature-showcase-app');
        document.body.appendChild(appElement);
        await testHandler.whenCalled('nextStepShown');

        // Advance through the first two steps to reach the last step.
        for (const selector
                 of ['feature-showcase-default-browser-step',
                     'feature-showcase-themes-and-customization-step']) {
          testHandler.resetResolver('nextStepShown');
          appElement.shadowRoot.querySelector(selector)!.dispatchEvent(
              new CustomEvent('step-completed'));
          await testHandler.whenCalled('nextStepShown');
        }

        // Complete the last step; finishFeatureShowcase must wait for the
        // stepper's checkmark animation to finish.
        const lastStep = appElement.shadowRoot.querySelector(
            'feature-showcase-password-manager-step');
        lastStep!.dispatchEvent(new CustomEvent('step-completed'));
        await microtasksFinished();

        assertEquals(0, testHandler.getCallCount('finishFeatureShowcase'));

        const stepper = lastStep!.querySelector('feature-showcase-stepper');
        const animation =
            stepper?.shadowRoot.querySelectorAll('.step')[2]?.querySelector(
                'cr-lottie');
        assertTrue(!!animation);

        animation.dispatchEvent(new CustomEvent('cr-lottie-completed'));
        await testHandler.whenCalled('finishFeatureShowcase');
      });
});

suite('FeatureShowcaseStepperTest', function() {
  let originalMatchMedia: (query: string) => MediaQueryList;
  let fakeMediaQueryList: FakeMediaQueryList;

  function createStepper(
      steps: string[], activeIndex: number): FeatureShowcaseStepperElement {
    const element = document.createElement('feature-showcase-stepper');
    element.steps = steps;
    element.activeIndex = activeIndex;
    document.body.appendChild(element);
    return element;
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // The stepper reads window.matchMedia while it is being constructed, so
    // the fake has to be in place before any stepper is created.
    originalMatchMedia = window.matchMedia;
    fakeMediaQueryList = new FakeMediaQueryList('(forced-colors: active)');
    window.matchMedia = () => fakeMediaQueryList;
  });

  teardown(function() {
    window.matchMedia = originalMatchMedia;
  });

  test('renders single ball for less than 3 steps', async function() {
    const stepperElement = createStepper(['step1', 'step2'], 0);
    await microtasksFinished();

    const steps = stepperElement.shadowRoot.querySelectorAll('.step');
    assertEquals(1, steps.length);
    const img = steps[0]?.querySelector('img');
    assertTrue(!!img);
    assertTrue(img.src.includes('product-logo.svg'));
  });

  test('renders stepper for 3 or more steps', async function() {
    const stepperElement = createStepper(['step1', 'step2', 'step3'], 1);
    await microtasksFinished();

    const steps = stepperElement.shadowRoot.querySelectorAll('.step');
    assertEquals(3, steps.length);

    // Completed step animates its checkmark, then settles on the static icon.
    const animation = steps[0]?.querySelector('cr-lottie');
    assertTrue(!!animation);
    animation.dispatchEvent(new CustomEvent('cr-lottie-completed'));
    await microtasksFinished();

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

  test('swaps the animation for a static icon', async function() {
    const stepperElement = createStepper(['step1', 'step2', 'step3'], 1);
    await microtasksFinished();

    const step = stepperElement.shadowRoot.querySelectorAll('.step')[0];
    const animation = step?.querySelector('cr-lottie');
    assertTrue(!!animation);
    assertFalse(!!step?.querySelector('cr-icon'));

    animation.dispatchEvent(new CustomEvent('cr-lottie-completed'));
    await microtasksFinished();

    assertFalse(!!step?.querySelector('cr-lottie'));
    const icon = step?.querySelector('cr-icon');
    assertTrue(!!icon);
    assertEquals('cr:check', icon.icon);
  });

  test('only the newly completed step animates', async function() {
    const stepperElement = createStepper(['step1', 'step2', 'step3'], 1);
    await microtasksFinished();

    const animation = stepperElement.shadowRoot.querySelectorAll(
                                        '.step')[0]?.querySelector('cr-lottie');
    assertTrue(!!animation);
    animation.dispatchEvent(new CustomEvent('cr-lottie-completed'));
    await microtasksFinished();

    stepperElement.activeIndex = 2;
    await microtasksFinished();

    // The step that already animated keeps its static icon, and only the step
    // that just completed animates.
    const steps = stepperElement.shadowRoot.querySelectorAll('.step');
    assertFalse(!!steps[0]?.querySelector('cr-lottie'));
    assertTrue(!!steps[0]?.querySelector('cr-icon'));
    assertTrue(!!steps[1]?.querySelector('cr-lottie'));
  });

  test('at most one step animates when advancing quickly', async function() {
    const stepperElement = createStepper(['step1', 'step2', 'step3'], 1);
    await microtasksFinished();

    const firstStep = stepperElement.shadowRoot.querySelectorAll('.step')[0];
    assertTrue(!!firstStep?.querySelector('cr-lottie'));

    // Advance before the first checkmark has finished drawing.
    stepperElement.activeIndex = 2;
    await microtasksFinished();

    // The unfinished animation gives up its place to the static icon rather
    // than drawing alongside the new one.
    const steps = stepperElement.shadowRoot.querySelectorAll('.step');
    assertFalse(!!steps[0]?.querySelector('cr-lottie'));
    assertTrue(!!steps[0]?.querySelector('cr-icon'));
    assertTrue(!!steps[1]?.querySelector('cr-lottie'));
  });

  test('renders a static checkmark in forced colors', async function() {
    fakeMediaQueryList.matches = true;
    const stepperElement = createStepper(['step1', 'step2', 'step3'], 1);
    await microtasksFinished();

    const step = stepperElement.shadowRoot.querySelectorAll('.step')[0];
    assertFalse(!!step?.querySelector('cr-lottie'));
    assertTrue(!!step?.querySelector('cr-icon'));
  });

  test('stays static when forced colors turns off', async function() {
    fakeMediaQueryList.matches = true;
    const stepperElement = createStepper(['step1', 'step2', 'step3'], 1);
    await microtasksFinished();

    const step = stepperElement.shadowRoot.querySelectorAll('.step')[0];
    assertTrue(!!step?.querySelector('cr-icon'));

    fakeMediaQueryList.matches = false;
    await microtasksFinished();

    // A checkmark that has already been drawn statically must not go back and
    // replay its animation.
    assertFalse(!!step?.querySelector('cr-lottie'));
    assertTrue(!!step?.querySelector('cr-icon'));
  });

  test(
      'waitForAnimationComplete resolves when forced colors turns on',
      async function() {
        const stepperElement = createStepper(['step1', 'step2', 'step3'], 1);
        await microtasksFinished();

        const completed = stepperElement.waitForAnimationComplete();
        await microtasksFinished();

        fakeMediaQueryList.matches = true;
        await completed;

        const step = stepperElement.shadowRoot.querySelectorAll('.step')[0];
        assertFalse(!!step?.querySelector('cr-lottie'));
        assertTrue(!!step?.querySelector('cr-icon'));
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
    passwordManagerProxyFactory.setInstance({handler: testHandler});

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
    themesAndCustomizationProxyFactory.setInstance({handler: testHandler});

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

suite('FeatureShowcaseGeminiStepTest', function() {
  let stepElement: FeatureShowcaseGeminiStepElement;
  let testHandler: TestMock<GeminiPageHandlerRemote>&GeminiPageHandlerRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testHandler = TestMock.fromClass(GeminiPageHandlerRemote);
    geminiProxyFactory.setInstance({handler: testHandler});

    stepElement = document.createElement('feature-showcase-gemini-step');
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

    await testHandler.whenCalled('acceptConsent');
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

    await testHandler.whenCalled('skipConsent');
    await stepCompletedEvent;
    assertEquals(0, testHandler.getCallCount('acceptConsent'));
  });
});
