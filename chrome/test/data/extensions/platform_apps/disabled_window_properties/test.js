// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: Some of the test code was put in the global scope on purpose!

var resultFromToolbarGetterAtStart = window.toolbar;

// The following statement implicitly invokes the window.toolbar setter.  This
// should delete the "disabler" getter and setter that were set up in
// chrome/renderer/resources/extensions/platform_app.js, and restore normal
// getter/setter behaviors from here on.
var toolbar = {blah: 'glarf'};

var resultFromToolbarGetterAfterRedefinition = window.toolbar;
var toolbarIsWindowToolbarAfterRedefinition = (toolbar === window.toolbar);

toolbar.blah = 'baz';

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.assertEq('undefined', typeof(resultFromToolbarGetterAtStart));
  chrome.test.assertEq('object',
      typeof(resultFromToolbarGetterAfterRedefinition));
  chrome.test.assertTrue(toolbarIsWindowToolbarAfterRedefinition);

  chrome.test.assertEq('baz', toolbar.blah);

  chrome.test.notifyPass();
});
