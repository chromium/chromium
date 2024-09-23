// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SettingsRowElement, SettingsSliderRowElement, SettingsSliderV2Element} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

suite(SettingsSliderRowElement.is, () => {
  let sliderRow: SettingsSliderRowElement;
  let internalRowElement: SettingsRowElement;
  let internalSliderElement: SettingsSliderV2Element;

  const ticks = [0, 5, 10, 15, 20];

  const fakePrefObject = {
    key: 'settings.pref',
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: 5,
  };

  /**
   * Assert the slider value matches the given value at the given `tickIndex`,
   * and the wrapped internal cr slider value matches the given index
   * `tickIndex`. Should only be called when using `ticks`.
   */
  function assertSliderValueByTick(tickIndex: number): void {
    assertEquals(ticks[tickIndex], internalSliderElement.value);
  }

  setup(async () => {
    clearBody();
    sliderRow = document.createElement(SettingsSliderRowElement.is);
    sliderRow.ticks = ticks;
    sliderRow.value = 5;
    document.body.appendChild(sliderRow);

    internalRowElement = sliderRow.$.internalRow;
    internalSliderElement = sliderRow.$.slider;
    await flushTasks();
  });

  suite('for internal row element', () => {
    test('internal slider element is slotted into the control slot', () => {
      const slotEl = strictQuery(
          'slot[name="control"]', internalRowElement.shadowRoot,
          HTMLSlotElement);
      const slottedElements = slotEl.assignedElements({flatten: true});
      assertEquals(1, slottedElements.length);
      assertEquals(internalSliderElement, slottedElements[0]);
    });

    test('label is passed to internal row', () => {
      const label = 'Lorem ipsum';
      sliderRow.label = label;
      assertEquals(label, internalRowElement.label);
    });

    test('sublabel is passed to internal row', () => {
      const sublabel = 'Lorem ipsum dolor sit amet';
      sliderRow.sublabel = sublabel;
      assertEquals(sublabel, internalRowElement.sublabel);
    });

    test('icon is passed to internal row', () => {
      const icon = 'os-settings:display';
      sliderRow.icon = icon;
      assertEquals(icon, internalRowElement.icon);
    });

    test('learn more URL is passed to internal row', () => {
      const learnMoreUrl = 'https://google.com';
      sliderRow.learnMoreUrl = learnMoreUrl;
      assertEquals(learnMoreUrl, internalRowElement.learnMoreUrl);
    });
  });

  suite('with disabled property', () => {
    test('should reflect to attribute', () => {
      // `disabled` is false by default.
      assertFalse(sliderRow.hasAttribute('disabled'));

      sliderRow.disabled = true;
      assertTrue(sliderRow.hasAttribute('disabled'));

      sliderRow.disabled = false;
      assertFalse(sliderRow.hasAttribute('disabled'));
    });

    test('is true if pref is enforced', () => {
      sliderRow.pref = {...fakePrefObject};
      assertFalse(sliderRow.disabled);

      sliderRow.pref = {
        ...fakePrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      assertTrue(sliderRow.disabled);
    });

    test('cannot be overridden if pref is enforced', () => {
      sliderRow.pref = {
        ...fakePrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      assertTrue(sliderRow.disabled);

      // Attempt to force enable the element. Element should still be disabled
      // since the pref is enforced.
      sliderRow.disabled = false;
      assertTrue(sliderRow.disabled);
    });

    test('slider is disabled', () => {
      assertFalse(internalSliderElement.disabled);

      internalSliderElement.disabled = true;
      assertTrue(internalSliderElement.disabled);
    });
  });

  suite('with pref', () => {
    setup(async () => {
      sliderRow.pref = {...fakePrefObject};
      await flushTasks();
    });

    test('pref value updates the slider value', async () => {
      ticks.forEach((tickValue, index) => {
        sliderRow.set('pref.value', tickValue);
        assertSliderValueByTick(index);
      });
    });

    test('policy indicator shows if pref is enforced', async () => {
      sliderRow.pref = {
        ...fakePrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      };
      await flushTasks();

      const policyIndicator = internalSliderElement.shadowRoot!.querySelector(
          'cr-policy-pref-indicator');
      assertTrue(isVisible(policyIndicator));
    });

    test('updating pref value dispatches pref change event', async () => {
      ticks.forEach(async (tickValue, index) => {
        const prefChangeEventPromise =
            eventToPromise('user-action-setting-pref-change', window);
        sliderRow.set('pref.value', tickValue);
        assertSliderValueByTick(index);

        const event = await prefChangeEventPromise;
        assertEquals(fakePrefObject.key, event.detail.prefKey);
        assertEquals(tickValue, event.detail.value);
      });
    });

    test('updating pref value dispatches change event', async () => {
      ticks.forEach(async (tickValue, index) => {
        const changeEventPromise = eventToPromise('change', window);
        sliderRow.set('pref.value', tickValue);
        assertSliderValueByTick(index);

        const event = await changeEventPromise;
        assertEquals(tickValue, event.detail.value);
        // Event should not pass the shadow DOM boundary.
        assertFalse(event.composed);
      });
    });
  });

  suite('without pref', () => {
    test('updating value updates the slider value', async () => {
      ticks.forEach((tickValue, index) => {
        sliderRow.value = tickValue;
        assertSliderValueByTick(index);
      });
    });

    test('updating value dispatches change event', async () => {
      ticks.forEach(async (tickValue, index) => {
        const changeEventPromise = eventToPromise('change', window);
        sliderRow.value = tickValue;
        assertSliderValueByTick(index);

        const event = await changeEventPromise;
        assertEquals(tickValue, event.detail);
        // Event should not pass the shadow DOM boundary.
        assertFalse(event.composed);
      });
    });
  });

  suite('focus()', async () => {
    test('should focus the slider element', async () => {
      assertNotEquals(
          internalSliderElement, sliderRow.shadowRoot!.activeElement);
      sliderRow.focus();
      assertEquals(internalSliderElement, sliderRow.shadowRoot!.activeElement);
    });
  });

  suite('for a11y', () => {
    test('label is the ARIA label by default', () => {
      const label = 'Lorem ipsum';
      sliderRow.label = label;
      assertEquals(label, internalSliderElement.ariaLabel);
    });

    test('sublabel is the ARIA description by default', () => {
      const sublabel = 'Lorem ipsum dolor sit amet';
      sliderRow.sublabel = sublabel;
      assertEquals(sublabel, internalSliderElement.ariaDescription);
    });

    test('ariaLabel property takes precedence for the ARIA label', () => {
      const label = 'Lorem ipsum';
      const ariaLabel = 'A11y ' + label;
      sliderRow.label = label;
      sliderRow.ariaLabel = ariaLabel;
      assertEquals(ariaLabel, internalSliderElement.ariaLabel);
    });

    test(
        'ariaDescription property takes precedence for the ARIA description',
        () => {
          const sublabel = 'Lorem ipsum dolor sit amet';
          const ariaDescription = 'A11y ' + sublabel;
          sliderRow.sublabel = sublabel;
          sliderRow.ariaDescription = ariaDescription;
          assertEquals(ariaDescription, internalSliderElement.ariaDescription);
        });
  });
});
