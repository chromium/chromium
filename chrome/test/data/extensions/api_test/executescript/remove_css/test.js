// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Original color.
const originalColor = 'rgb(255, 0, 0)';  // red

// Injected color.
const injectedColor = 'rgb(0, 128, 0)';  // green

// Second injected color.
const injectedColor2 = 'rgb(255, 255, 0)';  // yellow

// CSS to inject.
const code = '#main { color: green !important; }';
const file = '/file.css';

function getFrameIds(tabId, callback) {
  chrome.webNavigation.getAllFrames({tabId}, frames => {
    let sortedFrames = frames.sort(
        (a, b) => a.frameId < b.frameId ? -1 : a.frameId > b.frameId ? 1 : 0);

    // We make some assumptions about the frames returned.
    chrome.test.assertEq(5, sortedFrames.length);
    chrome.test.assertEq(sortedFrames[0].frameId, 0 /*main frame id*/);
    chrome.test.assertTrue(
        sortedFrames[1].frameId > 0 /* first non-main-frame id */);
    // We rely on the about:srcdoc being in index 4.
    // Currently this works because of the way frames are returned;
    // if that changes, we could filter to find the correct frames.
    chrome.test.assertEq('about:srcdoc', sortedFrames[4].url);

    callback(sortedFrames.map(frame => frame.frameId));
  });
}

// Helper called with either chrome.tab.insertCSS or chrome.tab.removeCSS
// as 'apiFunction'.
//
// 'details': Details of the CSS text to insert or remove.
// 'colorStateDelta': Delta of the expected value changes after 'apiFuncion'
//                    is called.
function makeCSSTester(apiFunction, tabId, frameIds, colorState) {
  return (details, colorStateDelta = []) => {
    return new Promise(resolve => {
      apiFunction(tabId, details, () => {
        chrome.test.assertNoLastError();
        // After the API call, modify the state with the given delta, then
        // verify that the state here reflects the actual state as reported
        // by the content script.
        let expected = [...Object.assign(colorState, colorStateDelta)];
        let pending = [];
        for (let frameId of frameIds) {
          pending.push(new Promise(messageResolve => {
            chrome.tabs.sendMessage(tabId, {}, {frameId}, color => {
              chrome.test.assertEq(expected.shift(), color);
              messageResolve();
            });
          }));
        }
        Promise.all(pending).then(resolve);
      });
    });
  };
}

chrome.test.getConfig(config => {
  let testUrl = 'http://example.com:' + config.testServer.port +
      '/extensions/api_test/executescript/remove_css/test.html';
  chrome.tabs.onUpdated.addListener(function listener(tabId, {status}) {
    if (status != 'complete')
      return;
    chrome.tabs.onUpdated.removeListener(listener);
    getFrameIds(tabId, frameIds => {
      // 'colorState' holds a snapshot of expected values of the CSS properties
      // being inserted/removed.
      // Values gets validated at the completion of the each test below.
      //
      // Each frame is a child of the frame preceding it. Frames 0 through 3
      // are <iframe src="..."> while frame 4 is <iframe srcdoc="...">
      // (about:srcdoc).
      let colorState = [
        originalColor, originalColor, originalColor, originalColor,
        originalColor
      ];
      let testInsertCSS =
          makeCSSTester(chrome.tabs.insertCSS, tabId, frameIds, colorState);
      let testRemoveCSS =
          makeCSSTester(chrome.tabs.removeCSS, tabId, frameIds, colorState);
      chrome.test.runTests([
        async function insertCSSShouldSucceed() {
          await testInsertCSS({code, allFrames: true, matchAboutBlank: true}, [
            injectedColor, injectedColor, injectedColor, injectedColor,
            injectedColor
          ]);
          chrome.test.succeed();
        },
        async function removeCSSShouldSucceed() {
          // When no frame ID is specified, the CSS is removed from the top
          // frame.
          await testRemoveCSS({code}, [originalColor, , , , , ]);
          chrome.test.succeed();
        },
        async function removeCSSWithDifferentCodeShouldDoNothing() {
          // If the specified code differs by even one character, it does not
          // match any inserted CSS and therefore nothing is removed.
          await testRemoveCSS({code: code + ' ', frameId: frameIds[0]});
          await testRemoveCSS({code: code + ' ', frameId: frameIds[1]});
          await testRemoveCSS({code: code + ' ', frameId: frameIds[2]});
          await testRemoveCSS({code: code + ' ', frameId: frameIds[3]});
          await testRemoveCSS(
              {code: code + ' ', frameId: frameIds[4], matchAboutBlank: true});
          chrome.test.succeed();
        },
        async function removeCSSWithDifferentCSSOriginShouldDoNothing() {
          // If only the CSS origin differs, nothing is removed.
          await testRemoveCSS({code, frameId: frameIds[1], cssOrigin: 'user'});
          chrome.test.succeed();
        },
        async function removeCSSWithFrameIdShouldSucceed() {
          // When a frame ID is specified, the CSS is removed from the given
          // frame.
          await testRemoveCSS({code, frameId: frameIds[1]},
                              [ , originalColor, , , , ]);
          chrome.test.succeed();
        },
        async function removeCSSWithAllFramesShouldSucceed() {
          // When "allFrames" is set to true, the CSS is removed from all
          // frames that match.
          //
          // By default "about:blank" and "about:srcdoc" frames do not match.
          await testRemoveCSS({code, allFrames: true},
                              [ , , originalColor, originalColor, , ]);
          chrome.test.succeed();
        },
        async function removeCSSWithMatchAboutBlankShouldSucceed() {
          // When "matchAboutBlank" is set to true, the CSS is also removed
          // from "about:blank" and "about:srcdoc" frames.
          await testRemoveCSS(
              {code, frameId: frameIds[4], matchAboutBlank: true},
              [, , , , originalColor]);
          chrome.test.succeed();
        },
        async function insertCSSWithFileShouldSucceed() {
          await testInsertCSS({file, allFrames: true},
            [injectedColor, injectedColor, injectedColor, injectedColor, , ]);
          chrome.test.succeed();
        },
        async function removeCSSWithFileShouldSucceed() {
          // When no frame ID is specified, the CSS is removed from the top
          // frame.
          await testRemoveCSS({file}, [originalColor, , , , , ]);
          chrome.test.succeed();
        },
        async function removeCSSWithDifferentFileShouldDoNothing() {
          // The CSS is not removed even though "/file.css" and "/other.css"
          // are identical.
          await testRemoveCSS({file: '/other.css', allFrames: true});
          chrome.test.succeed();
        },
        async function insertCSSWithDuplicateCodeShouldSucceed() {
          await testInsertCSS({ code }, [injectedColor, , , , , ])
          .then(() => testInsertCSS({ code: code.replace('green', 'yellow') },
                                    [injectedColor2, , , , , ]))
          .then(() => testInsertCSS({ code }, [injectedColor, , , , , ]));
          chrome.test.succeed();
        },
        async function removeCSSWithDuplicateCodeShouldSucceed() {
          await testRemoveCSS({ code }, [injectedColor2, , , , , ])
          // Only the last CSS that matches is removed.
          .then(() => testRemoveCSS({ code: code.replace('green', 'yellow') },
                                    [injectedColor, , , , , ]))
          .then(() => testRemoveCSS({ code }, [originalColor, , , , , ]));
          chrome.test.succeed();
        }
      ]);
    });
  });
  chrome.tabs.create({url: testUrl});
});
