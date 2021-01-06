// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  var classes = {true: 'success', false: 'failure'};
  var status = document.querySelector('.status');
  var summary = document.querySelector('.summary');
  var execution = summary.querySelector('.execution');
  var count = execution.querySelector('.count');
  var duration = execution.querySelector('.duration');
  var results = summary.querySelector('.results');
  var passed = results.querySelector('.passed');
  var failed = results.querySelector('.failed');
  var browser = document.querySelector('.browser');
  var details = document.querySelector('.details ol');
  var start = Date.now();
  var run = window.runTests(false);
  var end = Date.now();
  var counts = {passed: 0, failed: 0};
  var tests = [];
  var step;
  for (var i = 0; i < run.tests.length; ++i) {
    tests[i] = document.createElement('li');
    tests[i].setAttribute('class', classes[run.tests[i].success]);
    tests[i].appendChild(document.createElement('p'));
    tests[i].children[0].textContent = run.tests[i].name;
    tests[i].appendChild(document.createElement('ol'));
    counts.passed += run.tests[i].success ? 1 : 0;
    counts.failed += run.tests[i].success ? 0 : 1;
    for (var j = 0; j < run.tests[i].steps.length; ++j) {
      step = document.createElement('li');
      tests[i].children[1].appendChild(step);
      step.setAttribute('class', classes[run.tests[i].steps[j].success]);
      step.appendChild(document.createElement('p'));
      step.children[0].textContent = run.tests[i].steps[j].messages[0];
      for (var k = 1; k < run.tests[i].steps[j].messages.length; ++k) {
        step.appendChild(document.createElement('p'));
        step.children[k].textContent = run.tests[i].steps[j].messages[k];
        step.children[k].setAttribute('class', 'difference');
      }
    }
  }
  status.setAttribute('class', 'status ' + classes[run.success]);
  count.textContent = run.tests.length;
  duration.textContent = end - start;
  passed.textContent = counts.passed;
  passed.setAttribute('class', counts.passed ? 'passed' : 'passed none');
  failed.textContent = counts.failed;
  failed.setAttribute('class', counts.failed ? 'failed' : 'failed none');
  browser.textContent = window.navigator.userAgent;
  tests.forEach(function(test) { details.appendChild(test); });
}
