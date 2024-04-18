// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://site-engagement/app.js';

import type {SiteEngagementAppElement} from 'chrome://site-engagement/app.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('SiteEngagement', function() {
  const APP_URL = 'chrome://site-engagement/';
  const EXAMPLE_URL_1 = 'http://example.com/';
  const EXAMPLE_URL_2 = 'http://shmlexample.com/';

  let app: SiteEngagementAppElement;
  let cells: CellEntry[];

  interface CellEntry {
    origin: HTMLElement;
    scoreInput: HTMLInputElement;
    bonusScore: HTMLElement;
    totalScore: HTMLElement;
  }

  function getCells(): CellEntry[] {
    const originCells = Array.from(
        app.shadowRoot!.querySelectorAll<HTMLElement>('.origin-cell'));
    const scoreInputs =
        Array.from(app.shadowRoot!.querySelectorAll<HTMLInputElement>(
            '.base-score-input'));
    const bonusScoreCells = Array.from(
        app.shadowRoot!.querySelectorAll<HTMLElement>('.bonus-score-cell'));
    const totalScoreCells = Array.from(
        app.shadowRoot!.querySelectorAll<HTMLElement>('.total-score-cell'));
    return originCells.map((c, i) => {
      return {
        origin: c,
        scoreInput: scoreInputs[i]!,
        bonusScore: bonusScoreCells[i]!,
        totalScore: totalScoreCells[i]!,
      };
    });
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('site-engagement-app');
    document.body.appendChild(app);
    await app.whenPopulatedForTest();
    app.disableAutoupdate();
    cells = getCells();
  });

  test('check engagement values are loaded', function() {
    assertDeepEquals(
        [EXAMPLE_URL_1, EXAMPLE_URL_2], cells.map((c) => c.origin.textContent));
  });

  test('scores rounded to 2 decimal places', function() {
    assertDeepEquals(['10', '3.14'], cells.map(x => x.scoreInput.value));
    assertDeepEquals(['0', '0'], cells.map(x => x.bonusScore.textContent));
    assertDeepEquals(
        ['10', '3.14'], cells.map((x) => x.totalScore.textContent));
  });

  test('change score', async function() {
    const firstRow = cells[0]!;
    firstRow.scoreInput.value = '50';
    firstRow.scoreInput.dispatchEvent(new Event('change'));

    let {info} =
        await app.engagementDetailsProvider.getSiteEngagementDetails();
    info = info.filter(i => i.origin.url !== APP_URL);
    assertEquals(firstRow.origin.textContent, info[0]!.origin.url);
    assertEquals(50, info[0]!.baseScore);
  });

  test('show webui pages', async function() {
    const showWebUiPagesCheckbox =
        app.getRequiredElement<HTMLInputElement>(
            '#show-webui-pages-checkbox');
    showWebUiPagesCheckbox.click();
    const {info} =
        await app.engagementDetailsProvider.getSiteEngagementDetails();
    assertTrue(info.some(i => i.origin.url === APP_URL));
    assertDeepEquals(
        [EXAMPLE_URL_1, EXAMPLE_URL_2, APP_URL].toSorted(),
        getCells().map(c => c.origin.textContent).toSorted());
  });
});
