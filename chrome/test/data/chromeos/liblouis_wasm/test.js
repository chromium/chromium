// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pass = chrome.test.callbackPass;

const TABLE_NAME = 'en-us-comp8.ctb';
const CONTRACTED_TABLE_NAME = 'en-ueb-g2.ctb';
const TEXT = 'hello';
// Translation of the above string as a hexadecimal sequence of cells.
const CELLS = '1311070715';

let pendingCallback = null;
let pendingMessageId = -1;
let nextMessageId = 0;
let worker = null;

function loadLibrary(callback) {
  worker = new Worker('liblouis_wrapper.js');
  worker.addEventListener('message', function(e) {
    const reply = JSON.parse(e.data);
    console.log('Message from liblouis: ' + e.data);
    pendingCallback(reply);
  }, false /* useCapture */);

  rpc('load', {}, callback);
}

function rpc(command, args, callback) {
  const messageId = '' + nextMessageId++;
  args['command'] = command;
  args['message_id'] = messageId;
  const json = JSON.stringify(args);
  console.log('Message to liblouis: ' + json);
  worker.postMessage(json);
  pendingCallback = callback;
  pendingMessageId = messageId;
}


function expectSuccessReply(callback) {
  return function(reply) {
    chrome.test.assertEq(pendingMessageId, reply['in_reply_to']);
    chrome.test.assertTrue(reply['error'] === undefined);
    chrome.test.assertTrue(reply['success']);
    if (callback) {
      callback(reply);
    }
  };
}


loadLibrary(function() {
  chrome.test.runTests([
  function testGetTranslator() {
    rpc('CheckTable', { 'table_names': TABLE_NAME},
       pass(expectSuccessReply()));
  },

  function testTranslateString() {
    rpc('Translate', { 'table_names': TABLE_NAME, 'text': TEXT,
            form_type_map: []},
        pass(expectSuccessReply(function(reply) {
          chrome.test.assertEq(CELLS, reply['cells']);
        })));
  },

  // Regression test for the case where the translated result is more than
  // the double size of the input.  In this particular case, a single capital
  // letter 'T' should be translated to 3 cells in US English grade 2
  // braille (dots 56, 6, 2345).
  function testTranslateGrade2SingleCapital() {
    rpc('Translate', { 'table_names': 'en-us-g2.ctb', 'text': 'T',
                       form_type_map: []},
        pass(expectSuccessReply(function(reply) {
          chrome.test.assertEq('30201e', reply['cells']);
        })));
  },

  function testBackTranslateString() {
    rpc('BackTranslate', { 'table_names': TABLE_NAME, 'cells': CELLS},
        pass(expectSuccessReply(function(reply) {
          chrome.test.assertEq(TEXT, reply['text']);
        })));
  },

  // Backtranslate a one-letter contraction that expands to a much larger
  // string (k->knowledge).
  function testBackTranslateContracted() {
    rpc('BackTranslate', { 'table_names': CONTRACTED_TABLE_NAME,
                           'cells': '05'},  // dots 1 and 3
        pass(expectSuccessReply(function(reply) {
          chrome.test.assertEq('knowledge', reply['text']);
        })));
  },
]);
});
