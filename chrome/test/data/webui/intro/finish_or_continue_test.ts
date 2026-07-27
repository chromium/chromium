// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/finish_or_continue/app.js';

import {browserProxyFactory as finishOrContinueMojoProxyFactory, FinishOrContinuePageHandlerRemote} from 'chrome://intro/finish_or_continue.mojom-webui.js';
import type {FinishOrContinueAppElement} from 'chrome://intro/finish_or_continue/app.js';
import {MatchMediaProxyImpl} from 'chrome://intro/finish_or_continue/match_media_proxy.js';
import {IntroBrowserProxyImpl} from 'chrome://intro/intro_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isWindows} from 'chrome://resources/js/platform.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestIntroMojoBrowserProxy} from './test_intro_mojo_browser_proxy.js';
import {TestMatchMediaProxy} from './test_match_media_proxy.js';

suite('FinishOrContinueTest', function() {
  let testIntroMojoBrowserProxy: TestIntroMojoBrowserProxy;
  let testMatchMediaProxy: TestMatchMediaProxy;
  let handler: TestMock<FinishOrContinuePageHandlerRemote>&
      FinishOrContinuePageHandlerRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testIntroMojoBrowserProxy = new TestIntroMojoBrowserProxy();
    IntroBrowserProxyImpl.setInstance(testIntroMojoBrowserProxy);

    testMatchMediaProxy = new TestMatchMediaProxy();
    MatchMediaProxyImpl.setInstance(testMatchMediaProxy);

    handler = TestMock.fromClass(FinishOrContinuePageHandlerRemote);
    finishOrContinueMojoProxyFactory.setInstance({handler});

    loadTimeData.overrideValues({
      disableAnimations: false,
    });

    // Reset URL to default before each test to ensure isolation.
    const url = new URL(window.location.href);
    url.searchParams.delete('showcase');
    window.history.replaceState({}, '', url.toString());
  });

  async function createElement(): Promise<FinishOrContinueAppElement> {
    const element = document.createElement('finish-or-continue-app');
    document.body.appendChild(element);
    await microtasksFinished();
    return element;
  }

  test('ButtonsExist', async function() {
    const testElement = await createElement();
    assertTrue(!!testElement.$.continueEducationButton);
    assertTrue(!!testElement.$.startBrowsingButton);
    assertTrue(
        testElement.$.startBrowsingButton.classList.contains('action-button'));
  });

  test('ButtonsOrder', async function() {
    const testElement = await createElement();
    const buttonContainer = testElement.$.buttonContainer;
    assertTrue(!!buttonContainer);

    const buttons = buttonContainer.querySelectorAll('cr-button');
    assertEquals(2, buttons.length);

    if (isWindows) {
      assertEquals(testElement.$.startBrowsingButton, buttons[0]);
      assertEquals(testElement.$.continueEducationButton, buttons[1]);
    } else {
      assertEquals(testElement.$.continueEducationButton, buttons[0]);
      assertEquals(testElement.$.startBrowsingButton, buttons[1]);
    }
  });

  test('SeeWhatsNewButtonLabel_NoShowcaseParam', async function() {
    const testElement = await createElement();

    assertEquals(
        loadTimeData.getString('seeWhatsNewButtonLabel'),
        testElement.$.continueEducationButton.textContent.trim());
  });

  test('SeeWhatsNewButtonLabel_ShowcaseFalse', async function() {
    const url = new URL(window.location.href);
    url.searchParams.set('showcase', 'false');
    window.history.replaceState({}, '', url.toString());

    const testElement = await createElement();

    assertEquals(
        loadTimeData.getString('seeWhatsNewButtonLabel'),
        testElement.$.continueEducationButton.textContent.trim());
  });

  test('ContinueButtonLabel_ShowcaseTrue', async function() {
    const url = new URL(window.location.href);
    url.searchParams.set('showcase', 'true');
    window.history.replaceState({}, '', url.toString());

    const testElement = await createElement();

    assertEquals(
        loadTimeData.getString('seeMoreTipsButtonLabel'),
        testElement.$.continueEducationButton.textContent.trim());
  });

  test('AnimationsExistAndChangeWithTheme', async function() {
    const testElement = await createElement();
    const leftAnimation = testElement.$.leftAnimation;
    const rightAnimation = testElement.$.rightAnimation;
    const bottomAnimation = testElement.$.bottomAnimation;

    assertTrue(!!leftAnimation);
    assertTrue(!!rightAnimation);
    assertTrue(!!bottomAnimation);

    testMatchMediaProxy.setMatchMediaMatches(false);
    await microtasksFinished();

    assertTrue(leftAnimation.animationUrl.includes('light'));
    assertTrue(rightAnimation.animationUrl.includes('light'));
    assertTrue(bottomAnimation.animationUrl.includes('light'));

    testMatchMediaProxy.setMatchMediaMatches(true);
    await microtasksFinished();

    assertTrue(leftAnimation.animationUrl.includes('dark'));
    assertTrue(rightAnimation.animationUrl.includes('dark'));
    assertTrue(bottomAnimation.animationUrl.includes('dark'));
  });

  test('toggles animations', async function() {
    const testElement = await createElement();
    const leftAnimation = testElement.$.leftAnimation;
    const rightAnimation = testElement.$.rightAnimation;
    const bottomAnimation = testElement.$.bottomAnimation;

    let leftPlay: boolean|null = null;
    leftAnimation.setPlay = (play: boolean) => {
      leftPlay = play;
    };
    let rightPlay: boolean|null = null;
    rightAnimation.setPlay = (play: boolean) => {
      rightPlay = play;
    };
    let bottomPlay: boolean|null = null;
    bottomAnimation.setPlay = (play: boolean) => {
      bottomPlay = play;
    };

    const pageRemote =
        testIntroMojoBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    pageRemote.toggleAnimations(false);
    await pageRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(false, leftPlay);
    assertEquals(false, rightPlay);
    assertEquals(false, bottomPlay);

    pageRemote.toggleAnimations(true);
    await pageRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(true, leftPlay);
    assertEquals(true, rightPlay);
    assertEquals(true, bottomPlay);
  });

  test(
      'does not toggle animations when `disableAnimations` is true',
      async function() {
        loadTimeData.overrideValues({
          disableAnimations: true,
        });

        const testElement = await createElement();
        const leftAnimation = testElement.$.leftAnimation;

        let leftPlay: boolean|null = null;
        leftAnimation.setPlay = (play: boolean) => {
          leftPlay = play;
        };

        const pageRemote = testIntroMojoBrowserProxy.callbackRouter.$
                               .bindNewPipeAndPassRemote();
        pageRemote.toggleAnimations(false);
        await pageRemote.$.flushForTesting();
        await microtasksFinished();

        // Verify that setPlay was NOT called because animations are disabled.
        assertEquals(null, leftPlay);
      });

  test('StartBrowsingClicked', async function() {
    const testElement = await createElement();
    testElement.$.startBrowsingButton.click();
    await microtasksFinished();
    assertEquals(1, handler.getCallCount('startBrowsing'));
  });

  test('ContinueEducationClicked', async function() {
    const testElement = await createElement();
    testElement.$.continueEducationButton.click();
    await microtasksFinished();
    assertEquals(1, handler.getCallCount('continueEducation'));
  });
});
