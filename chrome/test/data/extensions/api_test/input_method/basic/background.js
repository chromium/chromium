// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const testParams = {
  initialInputMethod: '',
  newInputMethod: '',
  dictionaryLoaded: null,
};

// Wrap inputMethodPrivate in a promise-based API to simplify test code.
function wrapAsync(apiFunction) {
  return (...args) => {
    return new Promise((resolve, reject) => {
      apiFunction(...args, (...result) => {
        if (!!chrome.runtime.lastError) {
          reject(Error(chrome.runtime.lastError));
        } else {
          resolve(...result);
        }
      });
    });
  }
}

const asyncInputMethodPrivate = {
  getCurrentInputMethod:
      wrapAsync(chrome.inputMethodPrivate.getCurrentInputMethod),
  setCurrentInputMethod:
      wrapAsync(chrome.inputMethodPrivate.setCurrentInputMethod),
  getInputMethods:
      wrapAsync(chrome.inputMethodPrivate.getInputMethods),
  fetchAllDictionaryWords:
      wrapAsync(chrome.inputMethodPrivate.fetchAllDictionaryWords),
  addWordToDictionary:
      wrapAsync(chrome.inputMethodPrivate.addWordToDictionary)
};

chrome.test.runTests([
  // Queries the system for basic information needed for tests.
  // Needs to run first.
  async function initTests() {
    console.log('initTest: Getting initial inputMethod');

    const initialInputMethod =
        await asyncInputMethodPrivate.getCurrentInputMethod();
    const match = initialInputMethod.match(/_comp_ime_([a-z]{32})xkb:us::eng/);
    chrome.test.assertTrue(!!match);

    const extensionId = match[1];
    testParams.initialInputMethod = initialInputMethod;
    testParams.newInputMethod = `_comp_ime_${extensionId}xkb:fr::fra`;
    chrome.test.succeed();
  },

  async function setTest() {
    console.log(
        'setTest: Changing input method to: ' + testParams.newInputMethod);
    await asyncInputMethodPrivate.setCurrentInputMethod(
        testParams.newInputMethod);
    chrome.test.succeed();
  },

  async function getTest() {
    console.log('getTest: Getting current input method.');
    const inputMethod = await asyncInputMethodPrivate.getCurrentInputMethod();
    chrome.test.assertEq(testParams.newInputMethod, inputMethod);
    chrome.test.succeed();
  },

  async function observeTest() {
    console.log('observeTest: Adding input method event listener.');

    chrome.inputMethodPrivate.onChanged.addListener(
        function listener (inputMethod) {
          chrome.inputMethodPrivate.onChanged.removeListener(listener);
          chrome.test.assertEq(testParams.initialInputMethod, inputMethod);
          chrome.test.succeed();
        });

    console.log('observeTest: Changing input method to: ' +
                    testParams.initialInputMethod);
    await asyncInputMethodPrivate.setCurrentInputMethod(
        testParams.initialInputMethod);
  },

  async function setInvalidTest() {
    const kInvalidInputMethod = 'xx::xxx';
    console.log(
          'setInvalidTest: Changing input method to: ' + kInvalidInputMethod);
    asyncInputMethodPrivate.setCurrentInputMethod(kInvalidInputMethod)
        .catch(chrome.test.succeed);
  },

  async function getListTest() {
    console.log('getListTest: Getting input method list.');

    const inputMethods = await asyncInputMethodPrivate.getInputMethods();
    chrome.test.assertEq(7, inputMethods.length);

    chrome.test.assertTrue(
        inputMethods.some((im) => im.id == testParams.initialInputMethod));
    chrome.test.assertTrue(
        inputMethods.some((im) => im.id == testParams.newInputMethod));
    chrome.test.succeed();
  },

  async function loadDictionaryAsyncTest() {
    console.log('loadDictionaryAsyncTest: ');

    testParams.dictionaryLoaded = new Promise((resolve, reject) => {
      var message = 'before';
      chrome.inputMethodPrivate.onDictionaryLoaded.addListener(
        function listener () {
          chrome.inputMethodPrivate.onDictionaryLoaded.removeListener(listener);
          chrome.test.assertEq(message, 'after');
          resolve();
        });
      message = 'after';
    });
    // We don't need to wait for the promise to resolve before continuing since
    // promises are async wrappers.
    chrome.test.succeed();
  },

  async function fetchDictionaryTest() {
    await testParams.dictionaryLoaded;
    const words = await asyncInputMethodPrivate.fetchAllDictionaryWords();
    chrome.test.assertNe(undefined, words);
    chrome.test.assertEq(0, words.length);
    chrome.test.succeed();
  },

  async function addWordToDictionaryTest() {
    const wordToAdd = 'helloworld';
    await testParams.dictionaryLoaded;
    await asyncInputMethodPrivate.addWordToDictionary(wordToAdd);
    const words = await asyncInputMethodPrivate.fetchAllDictionaryWords();
    chrome.test.assertEq(1, words.length);
    chrome.test.assertEq(words[0], wordToAdd);
    chrome.test.succeed();
  },

  async function addDuplicateWordToDictionaryTest() {
    await testParams.dictionaryLoaded;
    asyncInputMethodPrivate.addWordToDictionary('helloworld')
        .catch(chrome.test.succeed);
  },

  async function dictionaryChangedTest() {
    const wordToAdd = 'helloworld2';
    await testParams.dictionaryLoaded;
    chrome.inputMethodPrivate.onDictionaryChanged.addListener(
      function listener(added, removed) {
        chrome.inputMethodPrivate.onDictionaryChanged.removeListener(listener);
        chrome.test.assertEq(1, added.length);
        chrome.test.assertEq(0, removed.length);
        chrome.test.assertEq(added[0], wordToAdd);
        chrome.test.succeed();
      });
    await asyncInputMethodPrivate.addWordToDictionary(wordToAdd);
  }
]);
