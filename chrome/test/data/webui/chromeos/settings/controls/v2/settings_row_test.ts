// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {getTrustedHTML, SettingsRowElement, SettingsToggleV2Element} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

suite(SettingsRowElement.is, () => {
  let rowElement: SettingsRowElement;

  async function createRowElement(html: string|TrustedHTML) {
    clearBody();
    document.body.innerHTML = html;
    rowElement = strictQuery('settings-row', document.body, SettingsRowElement);
    await flushTasks();
  }

  function getSlottedElements(slotName: string) {
    const slotEl = strictQuery(
        `slot[name="${slotName}"]`, rowElement.shadowRoot, HTMLSlotElement);
    return slotEl.assignedElements({flatten: true});
  }

  test('Label shows', async () => {
    await createRowElement(getTrustedHTML`
        <settings-row label="Lorem ipsum"></settings-row>`);

    const labelEl = strictQuery('#label', rowElement.shadowRoot, HTMLElement);
    assertTrue(isVisible(labelEl));
    assertEquals('Lorem ipsum', labelEl.innerText);
  });

  test('Sublabel shows', async () => {
    await createRowElement(getTrustedHTML`
        <settings-row sublabel="Lorem ipsum dolor sit amet"></settings-row>`);

    const sublabelEl =
        strictQuery('#sublabel', rowElement.shadowRoot, HTMLElement);
    assertTrue(isVisible(sublabelEl));
    assertEquals('Lorem ipsum dolor sit amet', sublabelEl.innerText);
  });

  suite('Learn more link', () => {
    function queryLearnMoreLink() {
      return rowElement.shadowRoot!.querySelector<HTMLAnchorElement>(
          '#learnMoreLink');
    }

    test(
        'Does not show if learn more URL property is not provided',
        async () => {
          await createRowElement(getTrustedHTML`<settings-row></settings-row>`);
          assertNull(queryLearnMoreLink());
        });

    test('Shows if learn more URL property is provided', async () => {
      await createRowElement(getTrustedHTML`
          <settings-row learn-more-url="https://google.com/"></settings-row>`);

      const learnMoreLink = queryLearnMoreLink();
      assertTrue(!!learnMoreLink);
      assertEquals('https://google.com/', learnMoreLink.href);
      assertEquals(
          loadTimeData.getString('learnMore'), learnMoreLink.innerText);
    });

    test('Opens in new tab and includes ARIA description', async () => {
      await createRowElement(getTrustedHTML`
          <settings-row learn-more-url="https://google.com/"></settings-row>`);

      const learnMoreLink = queryLearnMoreLink();
      assertTrue(!!learnMoreLink);
      assertEquals('_blank', learnMoreLink.target);
      assertEquals(
          loadTimeData.getString('opensInNewTab'),
          learnMoreLink.ariaDescription);
    });
  });

  suite('Leading icon', () => {
    function queryIcon() {
      return rowElement.shadowRoot!.querySelector('iron-icon');
    }

    test('Does not show if icon property is not provided', async () => {
      await createRowElement(getTrustedHTML`<settings-row></settings-row>`);
      assertNull(queryIcon());
    });

    test('Shows if icon property is provided', async () => {
      await createRowElement(getTrustedHTML`
          <settings-row icon="os-settings:display"></settings-row>`);

      const iconEl = queryIcon();
      assertTrue(!!iconEl);
      assertEquals('os-settings:display', iconEl.icon);
    });

    test('Can be slotted', async () => {
      await createRowElement(getTrustedHTML`
          <settings-row>
            <img slot="icon" src="#" alt="Slotted icon"></img>
          </settings-row>`);

      const slottedElements = getSlottedElements('icon');
      assertEquals(1, slottedElements.length);

      const slottedIcon = slottedElements[0];
      assertTrue(slottedIcon instanceof HTMLImageElement);
      assertEquals('Slotted icon', slottedIcon.alt);
    });
  });

  test('Control element(s) can be slotted', async () => {
    await createRowElement(getTrustedHTML`
        <settings-row>
          <settings-toggle-v2 slot="control" id="slottedToggle">
          </settings-toggle-v2>
        </settings-row>`);

    const slottedElements = getSlottedElements('control');
    assertEquals(1, slottedElements.length);

    const slottedToggle = slottedElements[0];
    assertTrue(slottedToggle instanceof SettingsToggleV2Element);
    assertEquals('slottedToggle', slottedToggle.id);
  });
});
