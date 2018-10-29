// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-slider. */
suite('SettingsSlider', function() {
  /** @type {!SettingsSliderElement} */
  let slider;

  /**
   * paper-slider instance wrapped by settings-slider.
   * @type {!CrSliderElement}
   */
  let crSlider;

  const ticks = [2, 4, 8, 16, 32, 64, 128];

  setup(function() {
    PolymerTest.clearBody();
    slider = document.createElement('settings-slider');
    slider.pref = {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 16,
    };
    document.body.appendChild(slider);
    crSlider = slider.$$('cr-slider');
  });

  function pressArrowRight() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 39, [], 'ArrowRight');
  }

  function pressArrowLeft() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 37, [], 'ArrowLeft');
  }

  function pressPageUp() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 33, [], 'PageUp');
  }

  function pressPageDown() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 34, [], 'PageDown');
  }

  function pressArrowUp() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 38, [], 'ArrowUp');
  }

  function pressArrowDown() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 40, [], 'ArrowDown');
  }

  function pressHome() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 36, [], 'Home');
  }

  function pressEnd() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 35, [], 'End');
  }

  test('enforce value', function() {
    // Test that the indicator is not present until after the pref is
    // enforced.
    indicator = slider.$$('cr-policy-pref-indicator');
    assertFalse(!!indicator);
    slider.pref = {
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 16,
    };
    Polymer.dom.flush();
    indicator = slider.$$('cr-policy-pref-indicator');
    assertTrue(!!indicator);
  });

  test('set value', function() {
    slider.ticks = ticks;
    slider.set('pref.value', 16);
    Polymer.dom.flush();
    expectEquals(6, crSlider.max);
    expectEquals(3, crSlider.value);

    // settings-slider only supports snapping to a range of tick values.
    // Setting to an in-between value should snap to an indexed value.
    slider.set('pref.value', 70);
    expectEquals(5, crSlider.value);
    expectEquals(64, slider.pref.value);

    // Setting the value out-of-range should clamp the slider.
    slider.set('pref.value', -100);
    expectEquals(0, crSlider.value);
    expectEquals(2, slider.pref.value);
  });

  test('move slider', function() {
    slider.ticks = ticks;
    slider.set('pref.value', 30);
    expectEquals(4, crSlider.value);

    pressArrowRight();
    expectEquals(5, crSlider.value);
    expectEquals(64, slider.pref.value);

    pressArrowRight();
    expectEquals(6, crSlider.value);
    expectEquals(128, slider.pref.value);

    pressArrowRight();
    expectEquals(6, crSlider.value);
    expectEquals(128, slider.pref.value);

    pressArrowLeft();
    expectEquals(5, crSlider.value);
    expectEquals(64, slider.pref.value);

    pressPageUp();
    expectEquals(6, crSlider.value);
    expectEquals(128, slider.pref.value);

    pressPageDown();
    expectEquals(5, crSlider.value);
    expectEquals(64, slider.pref.value);

    pressHome();
    expectEquals(0, crSlider.value);
    expectEquals(2, slider.pref.value);

    pressArrowDown();
    expectEquals(0, crSlider.value);
    expectEquals(2, slider.pref.value);

    pressArrowUp();
    expectEquals(1, crSlider.value);
    expectEquals(4, slider.pref.value);

    pressEnd();
    expectEquals(6, crSlider.value);
    expectEquals(128, slider.pref.value);
  });

  test('scaled slider', function() {
    slider.set('pref.value', 2);
    expectEquals(2, crSlider.value);

    slider.scale = 10;
    slider.max = 4;
    pressArrowRight();
    expectEquals(3, crSlider.value);
    expectEquals(.3, slider.pref.value);

    pressArrowRight();
    expectEquals(4, crSlider.value);
    expectEquals(.4, slider.pref.value);

    pressArrowRight();
    expectEquals(4, crSlider.value);
    expectEquals(.4, slider.pref.value);

    pressHome();
    expectEquals(0, crSlider.value);
    expectEquals(0, slider.pref.value);

    pressEnd();
    expectEquals(4, crSlider.value);
    expectEquals(.4, slider.pref.value);

    slider.set('pref.value', .25);
    expectEquals(2.5, crSlider.value);
    expectEquals(.25, slider.pref.value);

    pressPageUp();
    expectEquals(3.5, crSlider.value);
    expectEquals(.35, slider.pref.value);

    pressPageUp();
    expectEquals(4, crSlider.value);
    expectEquals(.4, slider.pref.value);
  });
});
