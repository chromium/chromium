// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js


const getControlledFrameClientHintsUserAgent =
    async function(controlledFrame) {
  const checkScript = `
  (() => {
    return navigator.userAgentData.brands.map((brand_version) => {
      return brand_version.brand;
    });
  })()
  `;
  return await executeAsyncScript(controlledFrame, checkScript);
}

const setClientHintsUABrandEnabledAndAwaitReload =
    async function(controlledframe, enable) {
  await new Promise((resolve, reject) => {
    if (!('src' in controlledframe)) {
      reject('FAIL');
      return;
    }
    controlledframe.addEventListener('loadstop', resolve);
    controlledframe.addEventListener('loadabort', reject);

    // |setClientHintsUABrandEnabled| should automatically reload.
    controlledframe.setClientHintsUABrandEnabled(enable);
  });
}

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');

  let brands = await getControlledFrameClientHintsUserAgent(controlledFrame);
  assert_true(brands.includes('ControlledFrame'));

  await setClientHintsUABrandEnabledAndAwaitReload(controlledFrame, false);
  brands = await getControlledFrameClientHintsUserAgent(controlledFrame);
  assert_false(brands.includes('ControlledFrame'));

  await setClientHintsUABrandEnabledAndAwaitReload(controlledFrame, true);
  brands = await getControlledFrameClientHintsUserAgent(controlledFrame);
  assert_true(brands.includes('ControlledFrame'));
}, 'Client Hints User Agent');
