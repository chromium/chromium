// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UtilitiesView} from 'chrome://net-internals/utilities_view.js';
import {$} from 'chrome://resources/js/util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {Task, TaskQueue} from './task_queue.js';
import {switchToView} from './test_util.js';

suite('UtilitiesViewTest', function() {
  /**
   * A Task that fills in the matcher and URL, clicks submit, and waits for
   * the observer to be notified.
   */
  class CheckUtilitiesResultTask extends Task {
    /**
     * @param {string} matcher The matcher pattern to test.
     * @param {string} url The URL to test.
     * @param {object} expectedResult The expected result object.
     */
    constructor(matcher, url, expectedResult) {
      super();
      this.matcher_ = matcher;
      this.url_ = url;
      this.expectedResult_ = expectedResult;
    }

    start() {
      UtilitiesView.getInstance().addObserverForTest(this);
      $(UtilitiesView.MATCHER_INPUT_ID).value = this.matcher_;
      $(UtilitiesView.URL_INPUT_ID).value = this.url_;
      $(UtilitiesView.TEST_SUBMIT_ID).click();
    }

    onTestMatcherResult(result) {
      if (this.isDone()) {
        return;
      }

      const outputText = $(UtilitiesView.TEST_OUTPUT_ID).innerText;
      assertTrue(
          outputText.includes(this.expectedResult_.text),
          `Expected output to contain "${
              this.expectedResult_.text}", but got "${outputText}"`);

      if (this.expectedResult_.expectedUrl) {
        const actualUrl = $(UtilitiesView.URL_INPUT_ID).value;
        assertEquals(
            this.expectedResult_.expectedUrl, actualUrl,
            `Expected URL input to be "${
                this.expectedResult_.expectedUrl}", but got "${actualUrl}"`);
      }

      this.running_ = false;
      // Start the next task asynchronously.
      window.setTimeout(this.onTaskDone.bind(this), 1);
    }
  }

  test('MatchSuccess', function() {
    switchToView('utilities');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckUtilitiesResultTask(
        'foobar.com', 'http://foobar.com', {text: 'Matched'}));
    return taskQueue.run();
  });

  test('MatchFailure', function() {
    switchToView('utilities');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckUtilitiesResultTask(
        'foobar.com', 'http://example.com', {text: 'Not Matched'}));
    return taskQueue.run();
  });

  test('InvalidMatcher', function() {
    switchToView('utilities');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckUtilitiesResultTask(
        'example.com:abc', 'http://foobar.com',
        {text: 'Invalid Matcher Format!'}));
    return taskQueue.run();
  });

  test('InvalidUrl', function() {
    switchToView('utilities');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckUtilitiesResultTask(
        'foobar.com', 'http://', {text: 'Invalid Sample URL Format!'}));
    return taskQueue.run();
  });

  test('DefaultSchemePrepended', function() {
    switchToView('utilities');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckUtilitiesResultTask(
        'foobar.com', 'foobar.com',
        {text: 'Matched', expectedUrl: 'http://foobar.com'}));
    return taskQueue.run();
  });
});
