// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/cubic_bezier.js';

import {CubicBezier} from 'chrome-untrusted://lens-overlay/cubic_bezier.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

// The tests for the cubic bezier easing calculations. Many of the tests in
// this file were translated from ui/gfx/geometry/cubic_bezier_unittest.cc.
suite('CubicBezier', function() {
  function expectNear(actual: number, expected: number, precision: number) {
    assertTrue(Math.abs(actual - expected) <= precision);
  }

  function validateSolver(fn: CubicBezier) {
    const precision = 0.00001;
    for (let t = 0; t <= 1; t += 0.05) {
      const x = fn.sampleCurveX(t);
      const root = fn.solveCurveX(x);
      expectNear(t, root, precision);
    }
  }

  test('Basic', () => {
    const fn = new CubicBezier(0.25, 0.0, 0.75, 1.0);
    const precision = 0.00015;
    expectNear(fn.solveForY(0), 0, precision);
    expectNear(fn.solveForY(0.05), 0.01136, precision);
    expectNear(fn.solveForY(0.1), 0.03978, precision);
    expectNear(fn.solveForY(0.15), 0.079780, precision);
    expectNear(fn.solveForY(0.2), 0.12803, precision);
    expectNear(fn.solveForY(0.25), 0.18235, precision);
    expectNear(fn.solveForY(0.3), 0.24115, precision);
    expectNear(fn.solveForY(0.35), 0.30323, precision);
    expectNear(fn.solveForY(0.4), 0.36761, precision);
    expectNear(fn.solveForY(0.45), 0.43345, precision);
    expectNear(fn.solveForY(0.5), 0.5, precision);
    expectNear(fn.solveForY(0.6), 0.63238, precision);
    expectNear(fn.solveForY(0.65), 0.69676, precision);
    expectNear(fn.solveForY(0.7), 0.75884, precision);
    expectNear(fn.solveForY(0.75), 0.81764, precision);
    expectNear(fn.solveForY(0.8), 0.87196, precision);
    expectNear(fn.solveForY(0.85), 0.92021, precision);
    expectNear(fn.solveForY(0.9), 0.96021, precision);
    expectNear(fn.solveForY(0.95), 0.98863, precision);
    expectNear(fn.solveForY(1), 1, precision);

    const basicUse = new CubicBezier(0.5, 1.0, 0.5, 1.0);
    assertEquals(0.875, basicUse.solveForY(0.5));

    const overshoot = new CubicBezier(0.5, 2.0, 0.5, 2.0);
    assertEquals(1.625, overshoot.solveForY(0.5));

    const undershoot = new CubicBezier(0.5, -1.0, 0.5, -1.0);
    assertEquals(-0.625, undershoot.solveForY(0.5));
  });

  test('CommonEasingFunctions', () => {
    const ease = new CubicBezier(0.25, 0.1, 0.25, 1);
    validateSolver(ease);
    const easeIn = new CubicBezier(0.42, 0, 1, 1);
    validateSolver(easeIn);
    const easeOut = new CubicBezier(0, 0, 0.58, 1);
    validateSolver(easeOut);
    const easeInOut = new CubicBezier(0.42, 0, 0.58, 1);
    validateSolver(easeInOut);
  });


  test('LinearEquivalentBeziers', () => {
    validateSolver(new CubicBezier(0.0, 0.0, 0.0, 0.0));
    validateSolver(new CubicBezier(1.0, 1.0, 1.0, 1.0));
  });

  test('ControlPointsOutsideUnitSquare', () => {
    validateSolver(new CubicBezier(0.3, 1.5, 0.8, 1.5));
    validateSolver(new CubicBezier(0.4, -0.8, 0.7, 1.7));
    validateSolver(new CubicBezier(0.7, -2.0, 1.0, -1.5));
    validateSolver(new CubicBezier(0, 4, 1, -3));
  });
});
