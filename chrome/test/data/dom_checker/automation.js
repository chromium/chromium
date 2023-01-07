/*
  Copyright 2009 The Chromium Authors
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

// Automation utilities.

function Automation() {
  this.test_count = 0;
  this.failures = [];
  this.done = false;
}

Automation.prototype.IncrementTestCount = function() {
  this.test_count++;
}

Automation.prototype.GetTestCount = function() {
  return this.test_count;
}

Automation.prototype.AddFailure = function(test) {
  // Remove any '<!-- NOP -->' that was inserted by DOM checker.
  test = test.replace(/<!-- NOP -->/, '');

  this.failures.push(test);
}

Automation.prototype.GetFailures = function() {
  return this.failures;
}

Automation.prototype.SetDone = function() {
  this.done = true;
}

Automation.prototype.IsDone = function() {
  return this.done;
}

automation = new Automation();

// Override functions that can spawn dialog boxes.

window.alert = function() {}
window.confirm = function() {}
window.prompt = function() {}
