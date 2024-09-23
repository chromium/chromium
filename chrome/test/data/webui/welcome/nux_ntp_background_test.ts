// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://welcome/ntp_background/nux_ntp_background.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {NtpBackgroundMetricsProxyImpl} from 'chrome://welcome/ntp_background/ntp_background_metrics_proxy.js';
import type {NtpBackgroundData} from 'chrome://welcome/ntp_background/ntp_background_proxy.js';
import {NtpBackgroundProxyImpl} from 'chrome://welcome/ntp_background/ntp_background_proxy.js';
import type {NuxNtpBackgroundElement} from 'chrome://welcome/ntp_background/nux_ntp_background.js';

import {TestMetricsProxy} from './test_metrics_proxy.js';
import {TestNtpBackgroundProxy} from './test_ntp_background_proxy.js';

suite('NuxNtpBackgroundTest', function() {
  const backgrounds: NtpBackgroundData[] = [
    {
      id: 0,
      title: 'Art',
      /* Image URLs are set to actual static images to prevent requesting
       * an external image. */
      imageUrl: './images/ntp_thumbnails/art.jpg',
      thumbnailClass: 'art',
    },
    {
      id: 1,
      title: 'Cityscape',
      imageUrl: './images/ntp_thumbnails/cityscape.jpg',
      thumbnailClass: 'cityscape',
    },
  ];

  let testElement: NuxNtpBackgroundElement;
  let testMetricsProxy: TestMetricsProxy;
  let testNtpBackgroundProxy: TestNtpBackgroundProxy;

  setup(function() {
    loadTimeData.overrideValues({
      ntpBackgroundDefault: 'Default',
    });

    testMetricsProxy = new TestMetricsProxy();
    NtpBackgroundMetricsProxyImpl.setInstance(testMetricsProxy);
    testNtpBackgroundProxy = new TestNtpBackgroundProxy();
    NtpBackgroundProxyImpl.setInstance(testNtpBackgroundProxy);
    testNtpBackgroundProxy.setBackgroundsList(backgrounds);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('nux-ntp-background');
    document.body.appendChild(testElement);

    testElement.onRouteEnter();
    return Promise.all([
      testMetricsProxy.whenCalled('recordPageShown'),
      testNtpBackgroundProxy.whenCalled('getBackgrounds'),
    ]);
  });

  teardown(function() {
    testElement.remove();
  });

  test('test displaying default and custom background', function() {
    const options = testElement.shadowRoot!.querySelectorAll('.option');
    assertEquals(3, options.length);

    // the first option should be the 'Default' option
    assertEquals(
        options[0]!.querySelector<HTMLDivElement>('.option-name')!.innerText,
        'Default');

    for (let i = 0; i < backgrounds.length; i++) {
      assertEquals(
          options[i + 1]!.querySelector<HTMLDivElement>(
                             '.option-name')!.innerText,
          backgrounds[i]!.title);
    }
  });

  test(
      'test previewing a background and going back to default',
      async function() {
        const options =
            testElement.shadowRoot!.querySelectorAll<HTMLButtonElement>(
                '.option');

        options[1]!.click();
        await Promise.all([
          microtasksFinished(),
          testNtpBackgroundProxy.whenCalled('preloadImage'),
        ]);
        assertEquals(
            testElement.$.backgroundPreview.style.backgroundImage,
            `url("${backgrounds[0]!.imageUrl}")`);
        assertTrue(
            testElement.$.backgroundPreview.classList.contains('active'));

        // go back to the default option, and pretend all CSS transitions
        // have completed
        options[0]!.click();
        await microtasksFinished();
        testElement.$.backgroundPreview.dispatchEvent(
            new Event('transitionend'));
        assertEquals(testElement.$.backgroundPreview.style.backgroundImage, '');
        assertFalse(
            testElement.$.backgroundPreview.classList.contains('active'));
      });

  test('test activating a background', async function() {
    const options =
        testElement.shadowRoot!.querySelectorAll<HTMLButtonElement>('.option');

    options[1]!.click();
    await microtasksFinished();
    assertFalse(options[0]!.hasAttribute('active'));
    assertTrue(options[1]!.hasAttribute('active'));
    assertFalse(options[2]!.hasAttribute('active'));
  });

  test('test setting the background when hitting next', async function() {
    // select the first non-default option and hit 'Next'
    const options =
        testElement.shadowRoot!.querySelectorAll<HTMLButtonElement>('.option');
    options[1]!.click();
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.action-button')!.click();
    const responses = await Promise.all([
      testMetricsProxy.whenCalled('recordChoseAnOptionAndChoseNext'),
      testNtpBackgroundProxy.whenCalled('setBackground'),
    ]);
    assertEquals(backgrounds[0]!.id, responses[1]);
  });

  test('test metrics for selecting an option and skipping', function() {
    const options =
        testElement.shadowRoot!.querySelectorAll<HTMLButtonElement>('.option');
    options[1]!.click();
    testElement.$.skipButton.click();
    return testMetricsProxy.whenCalled('recordChoseAnOptionAndChoseSkip');
  });

  test(
      'test metrics for when there is an error previewing the background',
      function() {
        testNtpBackgroundProxy.setPreloadImageSuccess(false);
        const options =
            testElement.shadowRoot!.querySelectorAll<HTMLButtonElement>(
                '.option');
        options[1]!.click();
        return testNtpBackgroundProxy.whenCalled(
            'recordBackgroundImageFailedToLoad');
      });

  test(
      `test metrics aren't sent when previewing the background is a success`,
      async function() {
        testNtpBackgroundProxy.setPreloadImageSuccess(true);
        const options =
            testElement.shadowRoot!.querySelectorAll<HTMLButtonElement>(
                '.option');
        options[1]!.click();
        await testNtpBackgroundProxy.whenCalled('preloadImage');
        assertEquals(
            0,
            testNtpBackgroundProxy.getCallCount(
                'recordBackgroundImageFailedToLoad'));
      });

  test('test metrics for doing nothing and navigating away', function() {
    testElement.onRouteUnload();
    return testMetricsProxy.whenCalled('recordDidNothingAndNavigatedAway');
  });

  test('test metrics for skipping', function() {
    testElement.$.skipButton.click();
    return testMetricsProxy.whenCalled('recordDidNothingAndChoseSkip');
  });

  test('test clearing the background when default is selected', function() {
    // select the default option and hit 'Next'
    const options =
        testElement.shadowRoot!.querySelectorAll<HTMLButtonElement>('.option');
    options[0]!.click();
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.action-button')!.click();
    return testNtpBackgroundProxy.whenCalled('clearBackground');
  });
});
