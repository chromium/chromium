// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {PdfOcrUserSelection, ScreenAiInstallStatus, SettingsTextToSpeechSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-text-to-speech-subpage>', function() {
  let page: SettingsTextToSpeechSubpageElement;
  let prefElement: SettingsPrefsElement;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-text-to-speech-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(function() {
    Router.getInstance().navigateTo(routes.A11Y_TEXT_TO_SPEECH);
  });

  teardown(function() {
    page.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  [{selector: '#ttsSubpageButton', route: routes.MANAGE_TTS_SETTINGS},
  ].forEach(({selector, route}) => {
    test(
        `should focus ${selector} button when returning from ${
            route.path} subpage`,
        async () => {
          await initPage();
          const router = Router.getInstance();

          const subpageButton =
              page.shadowRoot!.querySelector<HTMLElement>(selector);
          assert(subpageButton);

          subpageButton.click();
          assertEquals(route, router.currentRoute);
          assertNotEquals(
              subpageButton, page.shadowRoot!.activeElement,
              `${selector} should not be focused`);

          const popStateEventPromise = eventToPromise('popstate', window);
          router.navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(page);

          assertEquals(routes.A11Y_TEXT_TO_SPEECH, router.currentRoute);
          assertEquals(
              subpageButton, page.shadowRoot!.activeElement,
              `${selector} should be focused`);
        });
  });

  test('only allowed subpages are available in kiosk mode', async function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();

    const allowed_subpages = [
      'chromeVoxSubpageButton',
      'selectToSpeakSubpageButton',
      'ttsSubpageButton',
    ];

    const subpages = page.root!.querySelectorAll('cr-link-row');
    subpages.forEach(function(subpage) {
      if (isVisible(subpage)) {
        assertTrue(allowed_subpages.includes(subpage.id));
      }
    });
  });

  test(
      'pdf ocr pref enabled when both pdf ocr and screen reader enabled',
      async function() {
        // `features::kPdfOcr` is enabled in os_settings_v3_browsertest.js
        assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));

        await initPage();
        // Simulate enabling the ChromeVox.
        page.hasScreenReader = true;

        const pdfOcrToggle =
            page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#crosPdfOcrToggle');
        assertTrue(!!pdfOcrToggle);
        assertTrue(isVisible(pdfOcrToggle));
        // PDF OCR pref is on by default.
        assertTrue(pdfOcrToggle.checked);
        assertTrue(page.prefs.settings.a11y.pdf_ocr_always_active.value);
        pdfOcrToggle.click();

        await flushTasks();
        assertFalse(pdfOcrToggle.checked);
        assertFalse(page.prefs.settings.a11y.pdf_ocr_always_active.value);
      });

  test('pdf ocr toggle on invokes uma metric', async function() {
    // `features::kPdfOcr` is enabled in os_settings_v3_browsertest.js
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));

    const metrics = fakeMetricsPrivate();

    await initPage();
    // Simulate enabling the ChromeVox.
    page.hasScreenReader = true;

    const pdfOcrToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crosPdfOcrToggle');
    assertTrue(!!pdfOcrToggle);

    // PDF OCR is on by default, so need to turn off and then on again.
    pdfOcrToggle.click();
    await flushTasks();
    assertFalse(pdfOcrToggle.checked);
    // Turn on PDF OCR again.
    pdfOcrToggle.click();
    await flushTasks();
    assertTrue(pdfOcrToggle.checked);
    assertEquals(
        1,
        metrics.count(
            'Accessibility.PdfOcr.UserSelection',
            PdfOcrUserSelection.TURN_ON_ALWAYS_FROM_SETTINGS));
  });

  test('pdf ocr toggle off invokes uma metric', async function() {
    // `features::kPdfOcr` is enabled in os_settings_v3_browsertest.js
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));

    const metrics = fakeMetricsPrivate();

    await initPage();
    // Simulate enabling the ChromeVox.
    page.hasScreenReader = true;

    const pdfOcrToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crosPdfOcrToggle');
    // PDF OCR is on by default.
    assertTrue(!!pdfOcrToggle);
    assertTrue(pdfOcrToggle.checked);

    // Turn off PDF OCR.
    pdfOcrToggle.click();
    await flushTasks();
    assertFalse(pdfOcrToggle.checked);
    assertEquals(
        1,
        metrics.count(
            'Accessibility.PdfOcr.UserSelection',
            PdfOcrUserSelection.TURN_OFF_FROM_SETTINGS));
  });

  test('test pdf ocr toggle subtitle', async () => {
    // `features::kPdfOcr` is enabled in os_settings_v3_browsertest.js
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));

    await initPage();
    // Simulate enabling the ChromeVox.
    page.hasScreenReader = true;

    const pdfOcrToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crosPdfOcrToggle');
    assertTrue(!!pdfOcrToggle);
    await flushTasks();

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.NOT_DOWNLOADED);
    assertEquals(page.i18n('pdfOcrSubtitle'), pdfOcrToggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.FAILED);
    assertEquals(page.i18n('pdfOcrDownloadErrorLabel'), pdfOcrToggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.DOWNLOADING);
    assertEquals(page.i18n('pdfOcrDownloadingLabel'), pdfOcrToggle.subLabel);

    webUIListenerCallback('pdf-ocr-downloading-progress-changed', 50);
    assertEquals(
        page.i18n('pdfOcrDownloadProgressLabel', 50), pdfOcrToggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.DOWNLOADED);
    assertEquals(
        page.i18n('pdfOcrDownloadCompleteLabel'), pdfOcrToggle.subLabel);

    webUIListenerCallback('pdf-ocr-state-changed', ScreenAiInstallStatus.READY);
    assertEquals(page.i18n('pdfOcrSubtitle'), pdfOcrToggle.subLabel);
  });
});
