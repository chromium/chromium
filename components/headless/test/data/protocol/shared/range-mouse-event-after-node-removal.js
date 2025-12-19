// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(469041917): this is only required due to crbug.com/469041917 and
// should be removed when the issue is fixed.
//
// META: --disable-features=BoundaryEventDispatchTracksNodeRemoval

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests dispatch' +
      ' of mouse events to <input type="range"> after DOM node removal.');

  await dp.Runtime.enable();
  await session.evaluate(`
      document.open();
      document.write(
          '<input id="input" type="range" min="0" max="10" value="5">' +
          '</input>');
      document.close();
  `);

  const origVal =
      await session.evaluate(`document.getElementById('input').value`);
  testRunner.log(`Original value: ${origVal}`);
  const rect = JSON.parse(await session.evaluate(`
      JSON.stringify(document.getElementById('input').getBoundingClientRect())
  `));
  const origSliderPos = {
    x: rect.x + (rect.width >> 1),
    y: rect.y + (rect.height >> 1),
  };
  const finalSliderPos = {
    x: rect.x + rect.width - 3,
    y: rect.y + (rect.height >> 1),
  };
  await dp.Input.dispatchMouseEvent({
    ...origSliderPos,
    type: 'mouseMoved',
  });
  await dp.Input.dispatchMouseEvent({
    ...origSliderPos,
    type: 'mousePressed',
    button: 'left',
    buttons: 1,
    clickCount: 1,
  });
  await dp.Input.dispatchMouseEvent({
    ...finalSliderPos,
    type: 'mouseMoved',
    button: 'left',
    buttons: 1,
  });
  await dp.Input.dispatchMouseEvent({
    ...finalSliderPos,
    type: 'mouseReleased',
    button: 'left',
    buttons: 0,
    clickCount: 1,
  });
  const finalVal =
      await session.evaluate(`document.getElementById('input').value`);
  testRunner.log(`Final value: ${finalVal}`);

  testRunner.completeTest();
});
