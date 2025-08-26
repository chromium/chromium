// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {File} from 'chrome://new-tab-page/file_suggestion.mojom-webui.js';
import {RecommendationType} from 'chrome://new-tab-page/file_suggestion.mojom-webui.js';
import {FileSuggestionElement} from 'chrome://new-tab-page/lazy_load.js';
import type {CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FileSuggestion', () => {
  let fileSuggestion: FileSuggestionElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fileSuggestion = new FileSuggestionElement();
    document.body.appendChild(fileSuggestion);
  });

  function createFiles(
      numFiles: number, fileType: RecommendationType|null): File[] {
    const files: File[] = [];
    for (let i = 1; i < numFiles + 1; i++) {
      files.push({
        justificationText: `Edited ${i} days ago`,
        title: `${i} title`,
        id: `${i} id`,
        iconUrl: {url: `https://example.com/application/vnd.google-apps.${i}`},
        itemUrl: {url: 'https://${i}.com'},
        recommendationType: fileType,
      });
    }
    return files;
  }

  test('files render correctly', async () => {
    const numFiles = 2;
    fileSuggestion.files = createFiles(numFiles, /*fileType=*/ null);
    await microtasksFinished();

    const files =
        fileSuggestion.shadowRoot.querySelectorAll<HTMLElement>('.file');
    assertEquals(numFiles, files.length);

    const file1 = files[0];
    assertTrue(!!file1);
    assertTrue(isVisible(file1));
    const img1 = file1.querySelector<CrAutoImgElement>('.file-icon');
    assertTrue(!!img1);
    assertEquals(
        'https://example.com/application/vnd.google-apps.1', img1.autoSrc);
    const title1 = file1.querySelector<HTMLElement>('.file-title');
    assertTrue(!!title1);
    assertEquals('1 title', title1.textContent);
    const description1 = file1.querySelector<HTMLElement>('.file-description');
    assertTrue(!!description1);
    assertEquals('Edited 1 days ago', description1.textContent);

    const file2 = files[1];
    assertTrue(!!file2);
    assertTrue(isVisible(file2));
    const img2 = file2.querySelector<CrAutoImgElement>('.file-icon');
    assertTrue(!!img2);
    assertEquals(
        'https://example.com/application/vnd.google-apps.2', img2.autoSrc);
    const title2 = file2.querySelector<HTMLElement>('.file-title');
    assertTrue(!!title2);
    assertEquals('2 title', title2.textContent);
    const description2 = file2.querySelector<HTMLElement>('.file-description');
    assertTrue(!!description2);
    assertEquals('Edited 2 days ago', description2.textContent);
  });

  test(
      'clicking file dispatches `usage` event and records metric', async () => {
        const numFiles = 3;
        const moduleName = 'Foo';
        const metrics = fakeMetricsPrivate();
        fileSuggestion.moduleName = moduleName;
        fileSuggestion.files = createFiles(numFiles, /*fileType=*/ null);
        await microtasksFinished();

        let files =
            fileSuggestion.shadowRoot.querySelectorAll<HTMLElement>('.file');
        const clickIndex = 1;
        assertTrue(!!files[clickIndex]);
        let usagePromise = eventToPromise('usage', fileSuggestion);

        files[clickIndex].click();
        let usageEvent: Event = await usagePromise;
        assertTrue(!!usageEvent);
        await microtasksFinished();

        assertEquals(1, metrics.count(`NewTabPage.${moduleName}.FileClick`));
        assertEquals(
            1, metrics.count(`NewTabPage.${moduleName}.FileClick`, clickIndex));

        // Test that files with non-null `recommendationType` get a histogram
        // emission for the type.
        fileSuggestion.files =
            createFiles(numFiles, /*fileType=*/ RecommendationType.kTrending);
        await microtasksFinished();
        files =
            fileSuggestion.shadowRoot.querySelectorAll<HTMLElement>('.file');
        assertTrue(!!files[clickIndex]);
        usagePromise = eventToPromise('usage', fileSuggestion);
        files[clickIndex].click();
        usageEvent = await usagePromise;
        assertTrue(!!usageEvent);
        await microtasksFinished();
        assertEquals(
            1,
            metrics.count(
                `NewTabPage.${moduleName}.RecommendationTypeClick`,
                RecommendationType.kTrending));
      });
});
