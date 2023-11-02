// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Testing = {};

// A simple class which holds the test logic. Each "Test" instance has a |name|
// identifying the test, a |testLogic| which is the main body of the code for
// the test, and a |nextTest| which is a pointer to the test to run after
// |this|.
Testing.Test = function(name, testLogic) {
  this.name = name;
  this.testLogic = testLogic;
  this.nextTest = null;
};

// Links the next test to run to this test.
Testing.Test.prototype.setNextTest = function(nextTest) {
  this.nextTest = nextTest;
};

// This is how a test is invoked. The |onSuccess| and |onFailure| are callbacks
// which are called respectively when the test succeeds or fails. If the test
// succeeds and it has a |nextTest| assigned, then |nextTest| is involed and
// these callbacks are passed.
Testing.Test.prototype.run = function(onSuccess, onFailure) {
  console.log('Starting test "' + this.name + '"...');
  this.testLogic(function(success) {
    if (success) {
      console.log('Test "' + this.name + '" was successful.');
      if (this.nextTest) {
        this.nextTest.run(onSuccess, onFailure);
      } else {
        onSuccess();
      }
    } else {
      console.log('Test "' + this.name + '" failed.');
      onFailure();
    }
  }.bind(this));
};
