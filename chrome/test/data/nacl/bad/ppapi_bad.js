// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper routines for generating bad load tests.
// Webpage must have an 'embeds' div for injecting NaCl modules.
// Depends on nacltest.js.

function createModule(id, src, type) {
  return createNaClEmbed({
    id: id,
    src: src,
    width: 100,
    height: 20,
    type: type
  });
}


function addModule(module) {
  $('embeds').appendChild(module);
}


function removeModule(module) {
  $('embeds').removeChild(module);
}


function badLoadTest(tester, id, src, type, error_string) {
  tester.addAsyncTest(id, function(test){
    var module = createModule(id, src, type);

    test.expectEvent(module, 'load', function(e) {
      removeModule(module);
      test.fail('Module loaded successfully.');
    });
    test.expectEvent(module, 'error', function(e) {
      test.assertEqual(module.readyState, 4);
      if (error_string instanceof RegExp)
        test.assertRegexMatches(module.lastError, error_string);
      else
        test.assertEqual(module.lastError, error_string);
      test.expectEvent(module, 'loadend', function(e) {
        removeModule(module);
        test.pass();
      });
    });
    addModule(module);
  });
}
