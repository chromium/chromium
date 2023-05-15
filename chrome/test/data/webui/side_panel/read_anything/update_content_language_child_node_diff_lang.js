// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.\
//    UpdateContent_Language_ChildNodeDiffLang

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readAnything.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const container = readAnythingApp.getElementById('container');
  let result = true;

  const assertEquals = (actual, expected) => {
    const isEqual = actual === expected;
    if (!isEqual) {
      console.error(
          'Expected: ' + JSON.stringify(expected) + ', ' +
          'Actual: ' + JSON.stringify(actual));
    }
    result = result && isEqual;
    return isEqual;
  };

  const assertContainerInnerHTML = (expected) => {
    const actual = container.innerHTML;
    assertEquals(actual, expected);
  };

  // root htmlTag='#document' id=1
  // ++paragraph htmlTag='p' id=2 language='en'
  // ++++staticText name='This is in English' id=3
  // ++paragraph htmlTag='p' id=4 language='es'
  // ++++staticText name='Esto es en español' id=5
  // ++++link htmlTag='a' url='http://www.google.cn/' id=6 language='zh'
  // ++++++staticText name='This is a link in Chinese' id=7
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4],
      },
      {
        id: 2,
        role: 'paragraph',
        htmlTag: 'p',
        language: 'en',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is in English',
      },
      {
        id: 4,
        role: 'paragraph',
        htmlTag: 'p',
        language: 'es',
        childIds: [5, 6],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'Esto es en español',
      },
      {
        id: 6,
        role: 'link',
        htmlTag: 'a',
        language: 'zh',
        url: 'http://www.google.cn/',
        childIds: [7],
      },
      {
        id: 7,
        role: 'staticText',
        name: 'This is a link in Chinese',
      },
    ],
  };
  chrome.readAnything.setContentForTesting(axTree, [2, 4]);
  const expected = '<div><p lang="en">This is in English</p>' +
      '<p lang="es">Esto es en español' +
      '<a href="http://www.google.cn/" lang="zh">' +
      'This is a link in Chinese</a></p></div>';
  assertContainerInnerHTML(expected);

  return result;
})();
