// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  async function testTwoWindows() {
    const url1 = 'data:text/html,' +
        encodeURI('<div>Don\'t Click Me</div>' +
                  '<button>Click Me</button>');
    const url2 = 'data:text/html,' +
        encodeURI('<div>Don\'t Click Me too</div>' +
                  '<button>Click Me too</button>');
    const desktop =
        await new Promise(resolve => chrome.automation.getDesktop(resolve));
    chrome.windows.create({url: url1, focused: true});
    const button1 = await new Promise(
        resolve => {desktop.addEventListener(
            chrome.automation.EventType.LOAD_COMPLETE, (event) => {
              const button = desktop.find(
                  {attributes: {name: 'Click Me', role: 'button'}});
              if (button) {
                resolve(button);
              }
            })});

    const hitButton1 = await new Promise(
        resolve => desktop.hitTestWithReply(
            button1.location.left + 5, button1.location.top + 5, resolve));
    chrome.test.assertTrue(
        button1 === hitButton1 || button1 === hitButton1.parent);

    chrome.windows.create({url: url2, focused: true});
    const button2 = await new Promise(resolve => {
      desktop.addEventListener(
          chrome.automation.EventType.LOAD_COMPLETE, (event) => {
            const button = desktop.find(
                {attributes: {name: 'Click Me too', role: 'button'}});
            if (button) {
              resolve(button);
            }
          });
    });

    let hitButton2;
    while (!hitButton2 || hitButton2.name !== 'Click Me too') {
      // Note that we keep hit testing until we get the right node here. In
      // tests, where the machine is under a lot of load, it's rare but possible
      // that we get the 'Click Me' button back as a hit test from the browser
      // where not all windows have been updated yet.
      hitButton2 = await new Promise(
          resolve => desktop.hitTestWithReply(
              button2.location.left + 5, button2.location.top + 5, resolve));
    }

    // Note that the hit test might return either a static text or the button.
    chrome.test.assertTrue(
        hitButton2.role == chrome.automation.RoleType.BUTTON ||
        hitButton2.role == chrome.automation.RoleType.STATIC_TEXT);
    chrome.test.succeed();
  },
];

chrome.test.runTests(allTests);
