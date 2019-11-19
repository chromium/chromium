// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class TestEnv {
  constructor() {
    this.inputContext = null;
    this.surroundingText = '';
    this.compositionBounds = [];

    chrome.input.ime.onFocus.addListener((context) => {
      this.inputContext = context;
    });

    chrome.input.ime.onBlur.addListener(() => {
      this.inputContext = null;
    });

    chrome.input.ime.onSurroundingTextChanged.addListener(
        (_, surroundingInfo) => {
          this.surroundingText = surroundingInfo.text;
        });

    chrome.inputMethodPrivate.onCompositionBoundsChanged.addListener(
        (_, boundsList) => {
          this.compositionBounds = boundsList;
        });
  }

  getContextID() {
    return this.inputContext.contextID;
  }
};

function waitUntil(predicate) {
  return new Promise((resolve) => {
    const timer = setInterval(() => {
      if (predicate()) {
        clearInterval(timer);
        resolve();
      }
    }, 100);
  });
}

const testEnv = new TestEnv();

// Wrap inputMethodPrivate in a promise-based API to simplify test code.
function wrapAsync(apiFunction) {
  return (...args) => {
    return new Promise((resolve, reject) => {
      apiFunction(...args, (...result) => {
        if (!!chrome.runtime.lastError) {
          console.log(chrome.runtime.lastError.message);
          reject(Error(chrome.runtime.lastError));
        } else {
          resolve(...result);
        }
      });
    });
  }
}

const asyncInputIme = {
  commitText: wrapAsync(chrome.input.ime.commitText),
  setComposition: wrapAsync(chrome.input.ime.setComposition),
}

const asyncInputMethodPrivate = {
  setCurrentInputMethod:
      wrapAsync(chrome.inputMethodPrivate.setCurrentInputMethod),
  setCompositionRange:
      wrapAsync(chrome.inputMethodPrivate.setCompositionRange)
};

chrome.test.runTests([
  async function setUp() {
    await asyncInputMethodPrivate.setCurrentInputMethod(
        '_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest');

    chrome.test.succeed();
  },

  async function setCompositionRangeTest() {
    await asyncInputIme.commitText({
      contextID: testEnv.getContextID(),
      text: 'hello world'
    });

    await waitUntil(() => testEnv.surroundingText === 'hello world');

    // Cursor is at the end of the string.
    await asyncInputMethodPrivate.setCompositionRange({
      contextID: testEnv.getContextID(),
      selectionBefore: 5,
      selectionAfter: 0,
      segments: [
        { start: 0, end: 2, style: "underline" },
        { start: 2, end: 5, style: "underline" }
      ]
    });

    // Should underline "world".
    await waitUntil(() => testEnv.compositionBounds.length === 5);

    await asyncInputIme.setComposition({
      contextID: testEnv.getContextID(),
      text: "foo",
      cursor: 0
    });

    // Composition should change to "foo".
    await waitUntil(() => testEnv.compositionBounds.length === 3);

    // Should replace composition with "again".
    await asyncInputIme.commitText({
      contextID: testEnv.getContextID(),
      text: 'again'
    });

    await waitUntil(() => testEnv.surroundingText === 'hello again');

    // Cursor is at end of the string.
    // Call setCompositionRange with no segments.
    await asyncInputMethodPrivate.setCompositionRange({
      contextID: testEnv.getContextID(),
      selectionBefore: 5,
      selectionAfter: 0
    });

    // Composition should be "again".
    waitUntil(() => testEnv.compositionBounds.length === 5);

    // Should commit "again" and set composition to "in".
    await asyncInputMethodPrivate.setCompositionRange({
      contextID: testEnv.getContextID(),
      selectionBefore: 2,
      selectionAfter: 0
    });

    await waitUntil(() => testEnv.compositionBounds.length === 2);

    chrome.test.succeed();
  }
]);
