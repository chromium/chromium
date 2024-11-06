// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');

  const testScript = `(() => {
    return navigator.userAgentData.brands.map((brand_version) => {
      return brand_version.brand;
    });
  })()`;
  const brands = await executeAsyncScript(controlledFrame, testScript);

  assert_true(brands.includes('ControlledFrame'));
}, 'Client Hints User Agent');
