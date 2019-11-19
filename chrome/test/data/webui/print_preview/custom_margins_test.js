// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomMarginsOrientation, Margins, MarginsType, MeasurementSystem, MeasurementSystemUnitType, Size, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise, fakeDataBind} from 'chrome://test/test_util.m.js';

window.custom_margins_test = {};
custom_margins_test.suiteName = 'CustomMarginsTest';
/** @enum {string} */
custom_margins_test.TestNames = {
  ControlsCheck: 'controls check',
  SetFromStickySettings: 'set from sticky settings',
  DragControls: 'drag controls',
  SetControlsWithTextbox: 'set controls with textbox',
  SetControlsWithTextboxMetric: 'set controls with textbox metric',
  RestoreStickyMarginsAfterDefault: 'restore sticky margins after default',
  MediaSizeClearsCustomMargins: 'media size clears custom margins',
  LayoutClearsCustomMargins: 'layout clears custom margins',
  IgnoreDocumentMarginsFromPDF: 'ignore document margins from pdf',
  MediaSizeClearsCustomMarginsPDF: 'media size clears custom margins pdf',
  RequestScrollToOutOfBoundsTextbox: 'request scroll to out of bounds textbox',
  ControlsDisabledOnError: 'controls disabled on error',
};

suite(custom_margins_test.suiteName, function() {
  /** @type {?PrintPreviewMarginControlContainerElement} */
  let container = null;

  /** @type {?PrintPreviewModelElement} */
  let model = null;

  /** @type {!Array<!CustomMarginsOrientation>} */
  let sides = [];

  /** @type {!MeasurementSystem} */
  let measurementSystem = null;

  /** @type {number} */
  const pixelsPerInch = 100;

  /** @type {number} */
  const pointsPerInch = 72.0;

  /** @type {number} */
  const defaultMarginPts = 36;  // 0.5 inch

  // Keys for the custom margins setting, in order.
  const keys = ['marginTop', 'marginRight', 'marginBottom', 'marginLeft'];

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    measurementSystem =
        new MeasurementSystem(',', '.', MeasurementSystemUnitType.IMPERIAL);
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.set('settings.mediaSize.available', true);

    sides = [
      CustomMarginsOrientation.TOP, CustomMarginsOrientation.RIGHT,
      CustomMarginsOrientation.BOTTOM, CustomMarginsOrientation.LEFT
    ];

    container =
        document.createElement('print-preview-margin-control-container');
    container.previewLoaded = false;
    // 8.5 x 11, in points
    container.pageSize = new Size(612, 794);
    container.documentMargins = new Margins(
        defaultMarginPts, defaultMarginPts, defaultMarginPts, defaultMarginPts);
    container.state = State.NOT_READY;
  });

  /** @return {!Array<!PrintPreviewMarginControlElement>} */
  function getControls() {
    return container.shadowRoot.querySelectorAll(
        'print-preview-margin-control');
  }

  /*
   * Completes setup of the test by setting the settings and adding the
   * container to the document body.
   * @return {!Promise} Promise that resolves when all controls have been
   *     added and initialization is complete.
   */
  function finishSetup() {
    // Wait for the control elements to be created before updating the state.
    container.measurementSystem = measurementSystem;
    document.body.appendChild(container);
    const controlsAdded = eventToPromise('dom-change', container);
    return controlsAdded.then(() => {
      // 8.5 x 11, in pixels
      const controls = getControls();
      assertEquals(4, controls.length);
      container.settings = model.settings;
      fakeDataBind(model, container, 'settings');

      container.state = State.READY;
      container.updateClippingMask(new Size(850, 1100));
      container.updateScaleTransform(pixelsPerInch / pointsPerInch);
      container.previewLoaded = true;
      flush();
    });
  }

  /**
   * @param {!NodeList<!PrintPreviewMarginControlElement>} controls
   * @return {!Promise} Promise that resolves when transitionend has fired
   *     for all of the controls.
   */
  function getAllTransitions(controls) {
    return Promise.all(Array.from(controls).map(
        control => eventToPromise('transitionend', control)));
  }

  /**
   * Simulates dragging the margin control.
   * @param {!PrintPreviewMarginControlElement} control The control to move.
   * @param {number} start The starting position for the control in pixels.
   * @param {number} end The ending position for the control in pixels.
   */
  function dragControl(control, start, end) {
    if (window.getComputedStyle(control)['pointer-events'] === 'none') {
      return;
    }

    let xStart = 0;
    let yStart = 0;
    let xEnd = 0;
    let yEnd = 0;
    switch (control.side) {
      case CustomMarginsOrientation.TOP:
        yStart = start;
        yEnd = end;
        break;
      case CustomMarginsOrientation.RIGHT:
        xStart = control.clipSize.width - start;
        xEnd = control.clipSize.width - end;
        break;
      case CustomMarginsOrientation.BOTTOM:
        yStart = control.clipSize.height - start;
        yEnd = control.clipSize.height - end;
        break;
      case CustomMarginsOrientation.LEFT:
        xStart = start;
        xEnd = end;
        break;
      default:
        assertNotReached();
    }
    // Simulate events in the same order they are fired by the browser.
    // Need to provide a valid |pointerId| for setPointerCapture() to not
    // throw an error.
    control.dispatchEvent(new PointerEvent(
        'pointerdown', {pointerId: 1, clientX: xStart, clientY: yStart}));
    control.dispatchEvent(new PointerEvent(
        'pointermove', {pointerId: 1, clientX: xEnd, clientY: yEnd}));
    control.dispatchEvent(new PointerEvent(
        'pointerup', {pointerId: 1, clientX: xEnd, clientY: yEnd}));
  }

  /**
   * Tests setting the margin control with its textbox.
   * @param {!PrintPreviewMarginControlElement} control The control.
   * @param {string} key The control's key in the custom margin setting.
   * @param {number} currentValue The current margin value in points.
   * @param {string} input The new textbox input for the margin.
   * @param {boolean} invalid Whether the new value is invalid.
   * @param {number=} newValuePts the new margin value in pts. If not
   *     specified, computes the value assuming it is in bounds and assuming
   *     the default measurement system.
   * @return {!Promise} Promise that resolves when the test is complete.
   */
  function testControlTextbox(
      control, key, currentValuePts, input, invalid, newValuePts) {
    if (newValuePts === undefined) {
      newValuePts = invalid ? currentValuePts :
                              Math.round(parseFloat(input) * pointsPerInch);
    }
    assertEquals(
        currentValuePts, container.getSettingValue('customMargins')[key]);
    const controlTextbox = control.$.input;
    controlTextbox.value = input;
    controlTextbox.dispatchEvent(
        new CustomEvent('input', {composed: true, bubbles: true}));

    if (!invalid) {
      return eventToPromise('text-change', control).then(() => {
        assertEquals(
            newValuePts, container.getSettingValue('customMargins')[key]);
        assertFalse(control.invalid);
      });
    } else {
      return eventToPromise('input-change', control).then(() => {
        assertTrue(control.invalid);
      });
    }
  }

  /*
   * Initializes the settings custom margins to some test values, and returns
   * a map with the values.
   * @return {!Map<!CustomMarginsOrientation,
   *               number>}
   */
  function setupCustomMargins() {
    const orientationEnum = CustomMarginsOrientation;
    const marginValues = new Map([
      [orientationEnum.TOP, 72], [orientationEnum.RIGHT, 36],
      [orientationEnum.BOTTOM, 108], [orientationEnum.LEFT, 18]
    ]);
    model.settings.customMargins.value = {
      marginTop: marginValues.get(orientationEnum.TOP),
      marginRight: marginValues.get(orientationEnum.RIGHT),
      marginBottom: marginValues.get(orientationEnum.BOTTOM),
      marginLeft: marginValues.get(orientationEnum.LEFT),
    };
    return marginValues;
  }

  /*
   * Tests that the custom margins and margin value are cleared when the
   * setting |settingName| is set to have value |newValue|.
   * @param {string} settingName The name of the setting to check.
   * @param {*} newValue The value to set the setting to.
   * @return {!Promise} Promise that resolves when the check is complete.
   */
  function validateMarginsClearedForSetting(settingName, newValue) {
    const marginValues = setupCustomMargins();
    return finishSetup().then(() => {
      // Simulate setting custom margins.
      model.set('settings.margins.value', MarginsType.CUSTOM);

      // Validate control positions are set based on the custom values.
      const controls = getControls();
      controls.forEach((control, index) => {
        const side = sides[index];
        assertEquals(side, control.side);
        assertEquals(marginValues.get(side), control.getPositionInPts());
      });

      // Simulate setting the media size.
      container.setSetting(settingName, newValue);
      container.previewLoaded = false;

      // Margins should be reset to default and custom margins values should
      // be cleared.
      expectEquals(MarginsType.DEFAULT, container.getSettingValue('margins'));
      expectEquals(
          '{}', JSON.stringify(container.getSettingValue('customMargins')));

      // When preview loads, custom margins should still be empty, since
      // custom margins are not selected. We do not want to set the sticky
      // values until the user has selected custom margins.
      container.previewLoaded = true;
      expectEquals(
          '{}', JSON.stringify(container.getSettingValue('customMargins')));
    });
  }

  // Test that controls correctly appear when custom margins are selected and
  // disappear when the preview is loading.
  test(assert(custom_margins_test.TestNames.ControlsCheck), function() {
    return finishSetup()
        .then(() => {
          const controls = getControls();
          assertEquals(4, controls.length);

          // Controls are not visible when margin type DEFAULT is selected.
          controls.forEach(control => {
            assertEquals('0', window.getComputedStyle(control).opacity);
          });

          const onTransitionEnd = getAllTransitions(controls);
          // Controls become visible when margin type CUSTOM is selected.
          model.set('settings.margins.value', MarginsType.CUSTOM);

          // Wait for the opacity transitions to finish.
          return onTransitionEnd;
        })
        .then(function() {
          // Verify margins are correctly set based on previous value.
          assertEquals(
              defaultMarginPts,
              container.settings.customMargins.value.marginTop);
          assertEquals(
              defaultMarginPts,
              container.settings.customMargins.value.marginLeft);
          assertEquals(
              defaultMarginPts,
              container.settings.customMargins.value.marginRight);
          assertEquals(
              defaultMarginPts,
              container.settings.customMargins.value.marginBottom);

          // Verify there is one control for each side and that controls are
          // visible and positioned correctly.
          const controls = getControls();
          controls.forEach((control, index) => {
            assertFalse(control.invisible);
            assertFalse(control.disabled);
            assertEquals('1', window.getComputedStyle(control).opacity);
            assertEquals(sides[index], control.side);
            assertEquals(defaultMarginPts, control.getPositionInPts());
          });

          const onTransitionEnd = getAllTransitions(controls);

          // Disappears when preview is loading or an error message is shown.
          // Check that all the controls also disappear.
          container.previewLoaded = false;
          // Wait for the opacity transitions to finish.
          return onTransitionEnd;
        })
        .then(function() {
          const controls = getControls();
          controls.forEach((control, index) => {
            assertEquals('0', window.getComputedStyle(control).opacity);
            assertTrue(control.invisible);
            assertTrue(control.disabled);
          });
        });
  });

  // Tests that the margin controls can be correctly set from the sticky
  // settings.
  test(assert(custom_margins_test.TestNames.SetFromStickySettings), function() {
    return finishSetup().then(() => {
      const controls = getControls();

      // Simulate setting custom margins from sticky settings.
      model.set('settings.margins.value', MarginsType.CUSTOM);
      const marginValues = setupCustomMargins();
      model.notifyPath('settings.customMargins.value');
      flush();

      // Validate control positions have been updated.
      controls.forEach((control, index) => {
        const side = sides[index];
        assertEquals(side, control.side);
        assertEquals(marginValues.get(side), control.getPositionInPts());
      });
    });
  });

  // Test that dragging margin controls updates the custom margins setting.
  test(assert(custom_margins_test.TestNames.DragControls), function() {
    /**
     * Tests that the control can be moved from its current position (assumed
     * to be the default margins) to newPositionInPts by dragging it.
     * @param {!PrintPreviewMarginControlElement} control The control to test.
     * @param {number} index The index of this control in the container's list
     *     of controls.
     * @param {number} newPositionInPts The new position in points.
     */
    const testControl = function(control, index, newPositionInPts) {
      const oldValue = container.getSettingValue('customMargins');
      assertEquals(defaultMarginPts, oldValue[keys[index]]);

      // Compute positions in pixels.
      const oldPositionInPixels =
          defaultMarginPts * pixelsPerInch / pointsPerInch;
      const newPositionInPixels =
          newPositionInPts * pixelsPerInch / pointsPerInch;

      const whenDragChanged = eventToPromise('margin-drag-changed', container);
      dragControl(control, oldPositionInPixels, newPositionInPixels);
      return whenDragChanged.then(function() {
        const newValue = container.getSettingValue('customMargins');
        assertEquals(newPositionInPts, newValue[keys[index]]);
      });
    };

    return finishSetup().then(() => {
      const controls = getControls();
      model.set('settings.margins.value', MarginsType.CUSTOM);
      flush();


      // Wait for an animation frame. The position of the controls is set in
      // an animation frame, and this needs to be initialized before dragging
      // the control so that the computation of the new location is performed
      // with the correct initial margin offset.
      // Set all controls to 108 = 1.5" in points.
      window.requestAnimationFrame(function() {
        return testControl(controls[0], 0, 108)
            .then(testControl(controls[1], 1, 108))
            .then(testControl(controls[2], 2, 108))
            .then(testControl(controls[3], 3, 108));
      });
    });
  });

  /**
   * @param {!Array<!MarginControlElement>} controls
   * @param {number} currentValue Current margin value in pts
   * @param {string} input String to set in margin textboxes
   * @param {boolean} invalid Whether the string is invalid
   * @param {number=} newValuePts the new margin value in pts. If not
   *     specified, computes the value assuming it is in bounds and assuming
   *     the default measurement system.
   * @return {!Promise} Promise that resolves when all controls have been
   *     tested.
   */
  function testAllTextboxes(
      controls, currentValue, input, invalid, newValuePts) {
    return testControlTextbox(
               controls[0], keys[0], currentValue, input, invalid, newValuePts)
        .then(
            () => testControlTextbox(
                controls[1], keys[1], currentValue, input, invalid,
                newValuePts))
        .then(
            () => testControlTextbox(
                controls[2], keys[2], currentValue, input, invalid,
                newValuePts))
        .then(
            () => testControlTextbox(
                controls[3], keys[3], currentValue, input, invalid,
                newValuePts));
  }

  // Test that setting the margin controls with their textbox inputs updates
  // the custom margins setting.
  test(
      assert(custom_margins_test.TestNames.SetControlsWithTextbox), function() {
        return finishSetup().then(() => {
          const controls = getControls();
          // Set a shorter delay for testing so the test doesn't take too
          // long.
          controls.forEach(c => {
            c.getInput().setAttribute('data-timeout-delay', 1);
          });
          model.set('settings.margins.value', MarginsType.CUSTOM);
          flush();

          // Verify entering a new value updates the settings.
          // Then verify entering an invalid value invalidates the control
          // and does not update the settings.
          const value1 = '1.75';  // 1.75 inches
          const newMargin1 = Math.round(parseFloat(value1) * pointsPerInch);
          const value2 = '.6';
          const newMargin2 = Math.round(parseFloat(value2) * pointsPerInch);
          const value3 = '2';  // 2 inches
          const newMargin3 = Math.round(parseFloat(value3) * pointsPerInch);
          const maxTopMargin = container.pageSize.height - newMargin3 -
              72 /* MINIMUM_DISTANCE, see margin_control.js */;
          return testAllTextboxes(controls, defaultMarginPts, value1, false)
              .then(() => testAllTextboxes(controls, newMargin1, 'abc', true))
              .then(
                  () => testAllTextboxes(controls, newMargin1, '1.2abc', true))
              .then(
                  () => testAllTextboxes(controls, newMargin1, '1.   2', true))
              .then(() => testAllTextboxes(controls, newMargin1, value2, false))
              .then(() => testAllTextboxes(controls, newMargin2, value3, false))
              .then(
                  () => testControlTextbox(
                      controls[0], keys[0], newMargin3, '100', false,
                      maxTopMargin))
              .then(
                  () => testControlTextbox(
                      controls[0], keys[0], maxTopMargin, '1,000', false,
                      maxTopMargin));
        });
      });

  // Test that setting the margin controls with their textbox inputs updates
  // the custom margins setting, using a metric measurement system with a ','
  // as the decimal delimiter and '.' as the thousands delimiter. Regression
  // test for https://crbug.com/1005816.
  test(
      assert(custom_margins_test.TestNames.SetControlsWithTextboxMetric),
      function() {
        measurementSystem =
            new MeasurementSystem('.', ',', MeasurementSystemUnitType.METRIC);
        return finishSetup().then(() => {
          const controls = getControls();
          // Set a shorter delay for testing so the test doesn't take too
          // long.
          controls.forEach(c => {
            c.getInput().setAttribute('data-timeout-delay', 1);
          });
          model.set('settings.margins.value', MarginsType.CUSTOM);
          flush();

          // Verify entering a new value updates the settings.
          // Then verify entering an invalid value invalidates the control
          // and does not update the settings.
          const pointsPerMM = pointsPerInch / 25.4;
          const newMargin1 = '50,0';
          const newMargin1Pts = Math.round(50 * pointsPerMM);
          const newMargin2 = ',9';
          const newMargin2Pts = Math.round(.9 * pointsPerMM);
          const newMargin3 = '60';
          const newMargin3Pts = Math.round(60 * pointsPerMM);
          const maxTopMargin = container.pageSize.height - newMargin3Pts -
              72 /* MINIMUM_DISTANCE, see margin_control.js */;
          return testAllTextboxes(
                     controls, defaultMarginPts, newMargin1, false,
                     newMargin1Pts)
              .then(
                  () => testAllTextboxes(
                      controls, newMargin1Pts, 'abc', true, newMargin1Pts))
              .then(
                  () => testAllTextboxes(
                      controls, newMargin1Pts, '50,2abc', true, newMargin1Pts))
              .then(
                  () => testAllTextboxes(
                      controls, newMargin1Pts, '10,   2', true, newMargin1Pts))
              .then(
                  () => testAllTextboxes(
                      controls, newMargin1Pts, newMargin2, false,
                      newMargin2Pts))
              .then(
                  () => testAllTextboxes(
                      controls, newMargin2Pts, newMargin3, false,
                      newMargin3Pts))
              .then(
                  () => testControlTextbox(
                      controls[0], keys[0], newMargin3Pts, '1.000.000', false,
                      maxTopMargin))
              .then(
                  () => testControlTextbox(
                      controls[0], keys[0], maxTopMargin, '1.000', false,
                      maxTopMargin));
        });
      });

  // Test that if there is a custom margins sticky setting, it is restored
  // when margin setting changes.
  test(
      assert(custom_margins_test.TestNames.RestoreStickyMarginsAfterDefault),
      function() {
        const marginValues = setupCustomMargins();
        return finishSetup().then(() => {
          // Simulate setting custom margins.
          const controls = getControls();
          model.set('settings.margins.value', MarginsType.CUSTOM);

          // Validate control positions are set based on the custom values.
          controls.forEach((control, index) => {
            const side = sides[index];
            assertEquals(side, control.side);
            assertEquals(marginValues.get(side), control.getPositionInPts());
          });

          // Simulate setting minimum margins.
          model.set('settings.margins.value', MarginsType.MINIMUM);

          // Validate control positions still reflect the custom values.
          controls.forEach((control, index) => {
            const side = sides[index];
            assertEquals(side, control.side);
            assertEquals(marginValues.get(side), control.getPositionInPts());
          });
        });
      });

  // Test that if the media size changes, the custom margins are cleared.
  test(
      assert(custom_margins_test.TestNames.MediaSizeClearsCustomMargins),
      function() {
        return validateMarginsClearedForSetting(
                   'mediaSize', {height_microns: 200000, width_microns: 200000})
            .then(() => {
              // Simulate setting custom margins again.
              model.set('settings.margins.value', MarginsType.CUSTOM);

              // Validate control positions are initialized based on the default
              // values.
              const controls = getControls();
              controls.forEach((control, index) => {
                const side = sides[index];
                assertEquals(side, control.side);
                assertEquals(defaultMarginPts, control.getPositionInPts());
              });
            });
      });

  // Test that if the orientation changes, the custom margins are cleared.
  test(
      assert(custom_margins_test.TestNames.LayoutClearsCustomMargins),
      function() {
        return validateMarginsClearedForSetting('layout', true).then(() => {
          // Simulate setting custom margins again
          model.set('settings.margins.value', MarginsType.CUSTOM);

          // Validate control positions are initialized based on the default
          // values.
          const controls = getControls();
          controls.forEach((control, index) => {
            const side = sides[index];
            assertEquals(side, control.side);
            assertEquals(defaultMarginPts, control.getPositionInPts());
          });
        });
      });

  // Test that if the margins are not available, the custom margins setting is
  // not updated based on the document margins - i.e. PDFs do not change the
  // custom margins state.
  test(
      assert(custom_margins_test.TestNames.IgnoreDocumentMarginsFromPDF),
      function() {
        model.set('settings.margins.available', false);
        return finishSetup().then(() => {
          assertEquals(
              '{}', JSON.stringify(container.getSettingValue('customMargins')));
        });
      });

  // Test that if margins are not available but the user changes the media
  // size, the custom margins are cleared.
  test(
      assert(custom_margins_test.TestNames.MediaSizeClearsCustomMarginsPDF),
      function() {
        model.set('settings.margins.available', false);
        return validateMarginsClearedForSetting(
            'mediaSize', {height_microns: 200000, width_microns: 200000});
      });

  function whenAnimationFrameDone() {
    return new Promise(resolve => window.requestAnimationFrame(resolve));
  }

  // Test that if the user focuses a textbox that is not visible, the
  // text-focus event is fired with the correct values to scroll by.
  test(
      assert(custom_margins_test.TestNames.RequestScrollToOutOfBoundsTextbox),
      function() {
        return finishSetup()
            .then(() => {
              // Wait for the controls to be set up, which occurs in an
              // animation frame.
              return whenAnimationFrameDone();
            })
            .then(() => {
              const onTransitionEnd = getAllTransitions(getControls());

              // Controls become visible when margin type CUSTOM is selected.
              model.set('settings.margins.value', MarginsType.CUSTOM);
              container.notifyPath('settings.customMargins.value');
              flush();
              return onTransitionEnd;
            })
            .then(() => {
              // Zoom in by 2x, so that some margin controls will not be
              // visible.
              container.updateScaleTransform(pixelsPerInch * 2 / pointsPerInch);
              flush();
              return whenAnimationFrameDone();
            })
            .then(() => {
              const controls = getControls();
              assertEquals(4, controls.length);

              // Focus the bottom control, which is currently not visible since
              // the viewer is showing only the top left quarter of the page.
              const bottomControl = controls[2];
              const whenEventFired =
                  eventToPromise('text-focus-position', container);
              bottomControl.$.input.focus();
              // Workaround for mac so that this does not need to be an
              // interactive test: manually fire the focus event from the
              // control.
              bottomControl.fire('text-focus');
              return whenEventFired;
            })
            .then((args) => {
              // Shifts left by padding of 50px to ensure that the full textbox
              // is visible.
              assertEquals(50, args.detail.x);

              // Offset top will be 2097 = 200 px/in / 72 pts/in * (794pts -
              // 36ptx) - 9px radius of line
              // Height of the clip box is 200 px/in * 11in = 2200px
              // Shifts down by offsetTop = 2097 - height / 2 + padding =
              // 1047px. This will ensure that the textbox is in the visible
              // area.
              assertEquals(1047, args.detail.y);
            });
      });

  // Tests that the margin controls can be correctly set from the sticky
  // settings.
  test(
      assert(custom_margins_test.TestNames.ControlsDisabledOnError),
      function() {
        return finishSetup().then(() => {
          // Simulate setting custom margins.
          model.set('settings.margins.value', MarginsType.CUSTOM);

          const controls = getControls();
          controls.forEach(control => assertFalse(control.disabled));

          container.state = State.ERROR;
          // Validate controls are disabled.
          controls.forEach(control => assertTrue(control.disabled));

          container.state = State.READY;
          controls.forEach(control => assertFalse(control.disabled));
        });
      });
});
