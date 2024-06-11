// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrSliderElement, SettingsSliderV2Element} from 'chrome://os-settings/os_settings.js';
import {keyDownOn, keyUpOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

/** @fileoverview Suite of tests for settings-slider-v2. */
suite(SettingsSliderV2Element.is, () => {
  let slider: SettingsSliderV2Element;

  /**
   * cr-slider instance wrapped by settings-slider-v2.
   */
  let internalSlider: CrSliderElement;

  const ticks: number[] = [2, 4, 8, 16, 32, 64, 128];

  const fakePrefObject = {
    key: 'testPref',
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: 16,
  };

  function press(key: string) {
    keyDownOn(internalSlider, 0, [], key);
    keyUpOn(internalSlider, 0, [], key);
  }

  function pointerEvent(eventType: string, ratio: number) {
    const rect = internalSlider.shadowRoot!.querySelector<HTMLElement>(
                                         '#container')!.getBoundingClientRect();
    internalSlider.dispatchEvent(new PointerEvent(eventType, {
      buttons: 1,
      pointerId: 1,
      clientX: rect.left + (ratio * rect.width),
    }));
  }

  function pointerDown(ratio: number) {
    pointerEvent('pointerdown', ratio);
  }

  function pointerMove(ratio: number) {
    pointerEvent('pointermove', ratio);
  }

  function pointerUp() {
    // Ignores clientX for pointerup event.
    pointerEvent('pointerup', 0);
  }

  function assertCloseTo(actual: number, expected: number) {
    assertTrue(
        Math.abs(1 - actual / expected) <= Number.EPSILON,
        `expected ${expected} to be close to ${actual}`);
  }

  /**
   * Returns the internal slider element's value. If `ticks` is defined, it's
   * the index of the selected tick, not the tick value. Else it's the value of
   * the internal slider.
   */
  function getInternalSliderValue(): number {
    return internalSlider.value;
  }

  /**
   * Updates the value of the slider via downwards data-flow.
   */
  function updateSliderValue(hasPref: boolean, value: number) {
    if (hasPref) {
      slider.set('pref.value', value);
    } else {
      slider.value = value;
    }
  }

  /**
   * Assert the internal slider value (index) matches the given `tickIndex`.
   * Should only be called when using `ticks`.
   */
  function assertSliderValueByTick(tickIndex: number): void {
    assertEquals(tickIndex, getInternalSliderValue());
    assertEquals(ticks[tickIndex], slider.value);
  }

  suite('fundamental properties and functions', () => {
    setup(async () => {
      clearBody();
      slider = document.createElement(SettingsSliderV2Element.is);
      slider.value = 16;
      document.body.appendChild(slider);
      internalSlider = slider.shadowRoot!.querySelector('cr-slider')!;
      await flushTasks();
    });

    test('disabled slider if ticks has one value', () => {
      // Test that the slider is disabled even manually set disabled to false if
      // ticks has one value.
      assertFalse(slider.disabled);
      slider.disabled = false;
      slider.ticks = [2];

      flush();
      assertTrue(slider.disabled);
      assertEquals('true', internalSlider.ariaDisabled);
    });

    test('markers are shown by default when ticks is set', async () => {
      slider.ticks = ticks;
      flush();

      assertEquals(ticks.length, internalSlider.markerCount);
    });

    test('markers are hidden if number of ticks is greater than 10', () => {
      const longTicks: number[] =
          [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048];
      slider.ticks = longTicks;

      flush();
      assertEquals(0, internalSlider.markerCount);
    });

    test('explicitly set hideMarkers to true will hide markers', () => {
      slider.hideMarkers = true;
      slider.ticks = ticks;

      flush();
      assertEquals(0, internalSlider.markerCount);
    });

    [true, false].forEach(hideLabel => {
      test('visibility of labels', () => {
        slider.hideLabel = hideLabel;
        flush();

        const labels = slider.shadowRoot!.querySelector<HTMLElement>('#labels');
        assertTrue(!!labels);

        assertEquals(hideLabel, labels.hidden);
      });
    });

    test('should focus the internal slider', () => {
      assertNotEquals(
          internalSlider, slider.shadowRoot!.activeElement);
      slider.focus();
      assertEquals(
          internalSlider, slider.shadowRoot!.activeElement);
    });

    suite('for a11y', () => {
      test('ariaLabel property should apply to internal select', () => {
        const ariaLabel = 'A11y label';
        slider.ariaLabel = ariaLabel;
        assertEquals(ariaLabel, internalSlider.getAttribute('aria-label'));
      });

      test('ariaLabel property does not reflect to attribute', () => {
        const ariaLabel = 'A11y label';
        slider.ariaLabel = ariaLabel;
        assertFalse(slider.hasAttribute('aria-label'));
      });

      test('ariaDescription property should apply to internal select', () => {
        const ariaDescription = 'A11y description';
        slider.ariaDescription = ariaDescription;
        assertEquals(
            ariaDescription, internalSlider.getAttribute('aria-description'));
      });

      test('ariaDescription property does not reflect to attribute', () => {
        const ariaDescription = 'A11y description';
        slider.ariaDescription = ariaDescription;
        assertFalse(slider.hasAttribute('aria-description'));
      });

      test('A11y role description includes minLabel and maxLabel', () => {
        slider.minLabel = 'Low';
        slider.maxLabel = 'High';
        assertEquals('Slider: Low to High', internalSlider.ariaRoleDescription);
      });

      test('A11y role description is blank if no minLabel and maxLabel', () => {
        assertNull(internalSlider.ariaRoleDescription);
      });
    });
  });

  [true, false].forEach(hasPref => {
    suite(`${hasPref ? 'with' : 'without'} pref specified`, () => {
      setup(async () => {
        clearBody();
        slider = document.createElement(SettingsSliderV2Element.is);
        if (hasPref) {
          slider.pref = {...fakePrefObject};
        } else {
          slider.value = 16;
        }
        document.body.appendChild(slider);
        internalSlider = slider.shadowRoot!.querySelector('cr-slider')!;
        await flushTasks();
      });

      // Tests that should be run only when a pref is specified.
      if (hasPref) {
        test('disabled slider if pref is enforced', () => {
          // Test that the slider is disabled even manually set disabled to
          // false if the pref is enforced.
          assertFalse(slider.disabled);

          slider.pref = {
            ...fakePrefObject,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          };
          slider.disabled = false;

          flush();
          assertTrue(slider.disabled);
          assertEquals('true', internalSlider.ariaDisabled);
        });

        test(
            'indicator is not present until after the pref is enforced', () => {
              let indicator =
                  slider.shadowRoot!.querySelector('cr-policy-pref-indicator');
              assertFalse(isVisible(indicator));
              slider.pref = {
                ...fakePrefObject,
                controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
              };
              flush();
              indicator =
                  slider.shadowRoot!.querySelector('cr-policy-pref-indicator');
              assertTrue(isVisible(indicator));
            });

        test('pref value syncs to "value" property', () => {
          assertEquals(16, slider.value);

          slider.set('pref.value', 30);
          assertEquals(30, slider.value);

          slider.set('pref.value', 128);
          assertEquals(128, slider.value);
        });

        test('slider dispatches pref value change event', async () => {
          slider.ticks = ticks;
          updateSliderValue(/*hasPref=*/ true, /*newValue=*/ 32);

          const prefChangeEventPromise =
              eventToPromise('user-action-setting-pref-change', window);
          // Drag the knob on slider to the right. The next value on the right
          // should be 64.
          press('ArrowRight');
          const newValue = 64;

          const event = await prefChangeEventPromise;
          assertEquals(fakePrefObject.key, event.detail.prefKey);
          assertEquals(newValue, event.detail.value);
        });

        test('slider does not update the pref value directly', async () => {
          slider.ticks = ticks;
          const initialPrefValue = slider.pref!.value;

          const prefChangeEventPromise =
              eventToPromise('user-action-setting-pref-change', window);
          // Drag the knob on slider to the right.
          press('ArrowRight');
          await prefChangeEventPromise;

          // Local pref object should be treated as immutable data and should
          // not be updated directly.
          assertEquals(initialPrefValue, slider.pref!.value);
        });

        suite('Pref type validation', () => {
          [{
            prefType: chrome.settingsPrivate.PrefType.STRING,
            testValue: 'foo',
            isValid: false,
          },
           {
             prefType: chrome.settingsPrivate.PrefType.NUMBER,
             testValue: 1,
             isValid: true,
           },
           {
             prefType: chrome.settingsPrivate.PrefType.DICTIONARY,
             testValue: {},
             isValid: false,
           },
           {
             prefType: chrome.settingsPrivate.PrefType.BOOLEAN,
             testValue: true,
             isValid: false,
           },
           {
             prefType: chrome.settingsPrivate.PrefType.LIST,
             testValue: [],
             isValid: false,
           },
           {
             prefType: chrome.settingsPrivate.PrefType.URL,
             testValue: 'bar',
             isValid: false,
           },
          ].forEach(({prefType, testValue, isValid}) => {
            test(
                `${prefType} pref type is ${isValid ? 'valid' : 'invalid'}`,
                () => {
                  function validatePref() {
                    slider.pref = {
                      key: 'settings.sample',
                      type: prefType,
                      value: testValue,
                    };
                    slider.validatePref();
                  }

                  if (isValid) {
                    validatePref();
                  } else {
                    assertThrows(validatePref);
                  }
                });
          });
        });
      }

      test('slider dispatches a "change" event', async () => {
        slider.ticks = ticks;
        updateSliderValue(hasPref, /*newValue=*/ 32);

        const changeEventPromise = eventToPromise('change', window);
        press('ArrowRight');
        const newValue = 64;
        assertEquals(newValue, slider.value);

        const event = await changeEventPromise;
        assertEquals(newValue, event.detail);
        // Event should not pass the shadow DOM boundary.
        assertFalse(event.composed);
      });

      suite('with ticks', () => {
        setup(() => {
          slider.ticks = ticks;
        });

        test('Value updates via downwards data-flow', () => {
          ticks.forEach((tickValue, index) => {
            updateSliderValue(hasPref, /*newValue=*/ tickValue);
            assertSliderValueByTick(index);
          });
        });

        test('Value snaps to the range of tick values', () => {
          updateSliderValue(hasPref, /*newValue=*/ 70);
          assertSliderValueByTick(5);
        });

        test('Out-of-range values should clamp the slider', () => {
          updateSliderValue(hasPref, /*newValue=*/ -100);
          assertSliderValueByTick(0);

          updateSliderValue(hasPref, /*newValue=*/ 9001);
          assertSliderValueByTick(ticks.length - 1);
        });

        ['ArrowRight', 'PageUp', 'ArrowUp'].forEach((key) => {
          test(`Value increases on press ${key} key`, () => {
            updateSliderValue(hasPref, /*newValue=*/ 2);
            for (let i = 1; i < ticks.length; i++) {
              press(key);
              assertSliderValueByTick(i);
            }

            // Cannot move past the max value.
            press(key);
            assertSliderValueByTick(ticks.length - 1);
          });
        });

        ['ArrowLeft', 'PageDown', 'ArrowDown'].forEach((key) => {
          test(`Value decreases on press ${key} key`, () => {
            updateSliderValue(hasPref, /*newValue=*/ 128);
            for (let i = ticks.length - 2; i >= 0; i--) {
              press(key);
              assertSliderValueByTick(i);
            }

            // Cannot move past the min value.
            press(key);
            assertSliderValueByTick(0);
          });
        });

        test('Value goes to min value on press Home key', () => {
          updateSliderValue(hasPref, /*newValue=*/ 32);
          press('Home');
          assertSliderValueByTick(0);
        });

        test('Value goes to max value on press End key', () => {
          updateSliderValue(hasPref, /*newValue=*/ 4);
          press('End');
          assertSliderValueByTick(ticks.length - 1);
        });

        test('value updates instantly', () => {
          slider.updateValueInstantly = true;
          pointerDown(0);

          // Value is updated without calling pointerUp().
          pointerMove(3 / ticks.length);
          assertSliderValueByTick(3);

          // Value is updated without calling pointerUp().
          pointerMove(2 / ticks.length);
          assertSliderValueByTick(2);

          pointerUp();
          assertSliderValueByTick(2);
        });

        test('value updates after drag is done', () => {
          slider.updateValueInstantly = false;
          const initialValue = 4;
          updateSliderValue(hasPref, /*newValue=*/ initialValue);

          pointerDown(3 / ticks.length);
          assertEquals(initialValue, slider.value);
          assertEquals(3, getInternalSliderValue());

          // Pref value updates when dragging is done.
          pointerUp();
          assertEquals(16, slider.value);
          assertEquals(3, getInternalSliderValue());
        });
      });

      suite('with scale', () => {
        setup(() => {
          // Valid values for the slider are [0, 1].
          // Valid values for the internal slider are [0, 10].
          slider.min = 0;
          slider.max = 10;
          slider.scale = 10;
        });

        ['ArrowRight', 'PageUp', 'ArrowUp'].forEach((key) => {
          test(`Value increases on press ${key} key`, () => {
            updateSliderValue(hasPref, /*newValue=*/ 0.8);

            press(key);
            assertEquals(.9, slider.value);
            assertEquals(9, getInternalSliderValue());

            press(key);
            assertEquals(1, slider.value);
            assertEquals(10, getInternalSliderValue());

            // Cannot exceed the max.
            press(key);
            assertEquals(1, slider.value);
            assertEquals(10, getInternalSliderValue());
          });
        });

        ['ArrowLeft', 'PageDown', 'ArrowDown'].forEach((key) => {
          test(`Value decreases on press ${key} key`, () => {
            updateSliderValue(hasPref, /*newValue=*/ 0.2);

            press(key);
            assertEquals(.1, slider.value);
            assertEquals(1, getInternalSliderValue());

            press(key);
            assertEquals(0, slider.value);
            assertEquals(0, getInternalSliderValue());

            // Cannot exceed the min.
            press(key);
            assertEquals(0, slider.value);
            assertEquals(0, getInternalSliderValue());
          });
        });

        test('Value goes to min value on press Home key', () => {
          updateSliderValue(hasPref, /*newValue=*/ .5);
          press('Home');
          assertEquals(0, slider.value);
        });

        test('Value goes to max value on press End key', () => {
          updateSliderValue(hasPref, /*newValue=*/ .5);
          press('End');
          assertEquals(1, slider.value);
        });

        test('value updates instantly', () => {
          slider.updateValueInstantly = true;

          // Value is updated without calling pointerUp().
          pointerDown(.3);
          assertCloseTo(.3, slider.value);
          assertCloseTo(3, getInternalSliderValue());

          // Value is updated without calling pointerUp().
          pointerMove(.5);
          assertCloseTo(.5, slider.value);
          assertCloseTo(5, getInternalSliderValue());

          pointerUp();
          assertCloseTo(.5, slider.value);
          assertCloseTo(5, getInternalSliderValue());
        });

        test('value updates after drag is done', () => {
          slider.updateValueInstantly = false;
          const initialValue = .2;
          updateSliderValue(hasPref, initialValue);
          assertEquals(initialValue, slider.value);
          assertEquals(2, getInternalSliderValue());

          pointerDown(.5);
          assertEquals(initialValue, slider.value);
          assertCloseTo(5, getInternalSliderValue());

          pointerMove(.3);
          assertEquals(initialValue, slider.value);
          assertCloseTo(3, getInternalSliderValue());

          // Value updates when dragging is done.
          pointerUp();
          assertCloseTo(.3, slider.value);
          assertCloseTo(3, getInternalSliderValue());
        });
      });
    });
  });
});
