// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DomainSecurityPolicyView} from 'chrome://net-internals/domain_security_policy_view.js';
import {$} from 'chrome://resources/js/util.js';
import {assertEquals, assertLE, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

import {Task, TaskQueue} from './task_queue.js';
import {switchToView} from './test_util.js';

suite('DomainSecurityPolicyViewTest', function() {
  /*
   * Possible results of an HSTS query.
   * @enum {number}
   */
  const QueryResultType = {SUCCESS: 0, NOT_FOUND: 1, ERROR: 2};

  /**
   * A Task that waits for the results of a lookup query. Once the results are
   * received, checks them before completing.  Does not initiate the query.
   */
  class CheckQueryResultTask extends Task {
    /**
     * @param {string} domain The domain that was looked up.
     * @param {string} inputId The ID of the input element for the lookup
     domain.
     * @param {string} outputId The ID of the element where the results are
           presented.
     * @param {QueryResultType} queryResultType The expected result type of the
     *     results of the query.
     */
    constructor(domain, inputId, outputId, queryResultType) {
      super();
      this.domain_ = domain;
      this.inputId_ = inputId;
      this.outputId_ = outputId;
      this.queryResultType_ = queryResultType;
    }

    /**
     * Validates |result| and completes the task.
     * @param {object} result Results from the query.
     */
    onQueryResult_(result) {
      // Ignore results after |this| is finished.
      if (this.isDone()) {
        return;
      }

      assertEquals(this.domain_, $(this.inputId_).value);

      // Each case has its own validation function because of the design of the
      // test reporting infrastructure.
      if (result.error !== undefined) {
        this.checkError_(result);
      } else if (!result.result) {
        this.checkNotFound_(result);
      } else {
        this.checkSuccess_(result);
      }
      this.running_ = false;

      // Start the next task asynchronously, so it can add another observer
      // without getting the current result.
      window.setTimeout(this.onTaskDone.bind(this), 1);
    }

    /**
     * On errors, checks the result.
     * @param {object} result Results from the query.
     */
    checkError_(result) {
      assertEquals(QueryResultType.ERROR, this.queryResultType_);
      assertEquals(result.error, $(this.outputId_).innerText);
    }

    /**
     * Checks the result when the entry was not found.
     * @param {object} result Results from the query.
     */
    checkNotFound_(result) {
      assertEquals(QueryResultType.NOT_FOUND, this.queryResultType_);
      assertEquals('Not found', $(this.outputId_).innerText);
    }

    /**
     * Checks successful results.
     * @param {object} result Results from the query.
     */
    checkSuccess_(result) {
      assertEquals(QueryResultType.SUCCESS, this.queryResultType_);
      // Verify that the domain appears somewhere in the displayed text.
      const outputText = $(this.outputId_).innerText;
      assertLE(0, outputText.search(this.domain_));
    }
  }

  /**
   * A Task that waits for the results of an HSTS query. Once the results are
   * received, checks them before completing. Does not initiate the query.
   * @param {string} domain The domain expected in the returned results.
   * @param {bool} stsSubdomains Whether or not the stsSubdomains flag is
   *     expected to be set in the returned results.  Ignored on error and not
   *     found results.
   * @param {QueryResultType} queryResultType The expected result type of the
   *     results of the query.
   * @extends {CheckQueryResultTask}
   */
  class CheckHSTSQueryResultTask extends CheckQueryResultTask {
    constructor(domain, stsSubdomains, queryResultType) {
      super(
          domain, DomainSecurityPolicyView.QUERY_HSTS_INPUT_ID,
          DomainSecurityPolicyView.QUERY_HSTS_OUTPUT_DIV_ID, queryResultType);
      this.stsSubdomains_ = stsSubdomains;
    }

    /**
     * Starts watching for the query results.
     */
    start() {
      DomainSecurityPolicyView.getInstance().addHSTSObserverForTest(this);
    }

    /**
     * Callback from the BrowserBridge.  Validates |result| and completes the
     * task.
     * @param {object} result Results from the query.
     */
    onHSTSQueryResult(result) {
      this.onQueryResult_(result);
    }

    /**
     * Checks successful results.
     * @param {object} result Results from the query.
     */
    checkSuccess_(result) {
      assertEquals(this.stsSubdomains_, result.dynamic_sts_include_subdomains);
      super.checkSuccess_(result);
    }
  }

  /**
   * A Task to try and add an HSTS domain via the HTML form. The task will wait
   * until the results from the automatically sent query have been received, and
   * then checks them against the expected values.
   */
  class AddHSTSTask extends CheckHSTSQueryResultTask {
    /**
     * Fills out the add form, simulates a click to submit it, and starts
     * listening for the results of the query that is automatically submitted.
     */
    start() {
      super.start();
      $(DomainSecurityPolicyView.ADD_HSTS_INPUT_ID).value = this.domain_;
      $(DomainSecurityPolicyView.ADD_STS_CHECK_ID).checked =
          this.stsSubdomains_;
      $(DomainSecurityPolicyView.ADD_HSTS_SUBMIT_ID).click();
    }
  }

  /**
   * A Task to query a domain and wait for the results. Parameters mirror those
   * of CheckHSTSQueryResultTask, except |domain| is also the name of the domain
   * to query.
   */
  class QueryHSTSTask extends CheckHSTSQueryResultTask {
    /**
     * Fills out the query form, simulates a click to submit it, and starts
     * listening for the results.
     */
    start() {
      super.start();
      $(DomainSecurityPolicyView.QUERY_HSTS_INPUT_ID).value = this.domain_;
      $(DomainSecurityPolicyView.QUERY_HSTS_SUBMIT_ID).click();
    }
  }

  /**
   * Task that deletes a single domain, then queries the deleted domain to make
   * sure it's gone.
   */
  class DeleteTask extends CheckHSTSQueryResultTask {
    /**
     * @param {string} domain The domain to delete.
     * @param {QueryResultType} queryResultType The result of the query.  Can be
     *     QueryResultType.ERROR or QueryResultType.NOT_FOUND.
     */
    constructor(domain, queryResultType) {
      assertNotEquals(queryResultType, QueryResultType.SUCCESS);
      super(domain, false, queryResultType);
      this.domain_ = domain;
    }

    /**
     * Fills out the delete form and simulates a click to submit it.  Then sends
     * a query.
     */
    start() {
      // Adds the observer.
      super.start();
      // Fill out delete form.
      $(DomainSecurityPolicyView.DELETE_INPUT_ID).value = this.domain_;
      $(DomainSecurityPolicyView.DELETE_SUBMIT_ID).click();
      // Query
      $(DomainSecurityPolicyView.QUERY_HSTS_INPUT_ID).value = this.domain_;
      $(DomainSecurityPolicyView.QUERY_HSTS_SUBMIT_ID).click();
    }
  }

  /**
   * Checks that querying a domain that was never added fails.
   */
  test('QueryNotFound', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new QueryHSTSTask('somewhere.com', false, QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Checks that querying a domain with an invalid name returns an error.
   */
  test('QueryError', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new QueryHSTSTask('\u3024', false, QueryResultType.ERROR));
    return taskQueue.run();
  });

  /**
   * Deletes a domain that was never added.
   */
  test('DeleteNotFound', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Deletes a domain that returns an error on lookup.
   */
  test('DeleteError', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new DeleteTask('\u3024', QueryResultType.ERROR));
    taskQueue.run();
  });

  /**
   * Adds a domain and then deletes it.
   */
  test('AddDelete', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new AddHSTSTask('somewhere.com', false, QueryResultType.SUCCESS));
    taskQueue.addTask(
        new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Tries to add a domain with an invalid name.
   */
  test('AddFail', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new AddHSTSTask(
        '0123456789012345678901234567890' +
            '012345678901234567890123456789012345',
        false, QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Tries to add a domain with a name that errors out on lookup due to having
   * non-ASCII characters in it.
   */
  test('AddError', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new AddHSTSTask('\u3024', false, QueryResultType.ERROR));
    taskQueue.run();
  });

  /**
   * Adds the same domain twice in a row, modifying some values the second time.
   */
  test('AddOverwrite', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new AddHSTSTask('somewhere.com', true, QueryResultType.SUCCESS));
    taskQueue.addTask(
        new AddHSTSTask('somewhere.com', false, QueryResultType.SUCCESS));
    taskQueue.addTask(
        new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Adds two different domains and then deletes them.
   */
  test('AddTwice', function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new AddHSTSTask('somewhere.com', false, QueryResultType.SUCCESS));
    taskQueue.addTask(new QueryHSTSTask(
        'somewhereelse.com', false, QueryResultType.NOT_FOUND));
    taskQueue.addTask(
        new AddHSTSTask('somewhereelse.com', true, QueryResultType.SUCCESS));
    taskQueue.addTask(
        new QueryHSTSTask('somewhere.com', false, QueryResultType.SUCCESS));
    taskQueue.addTask(
        new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
    taskQueue.addTask(
        new QueryHSTSTask('somewhereelse.com', true, QueryResultType.SUCCESS));
    taskQueue.addTask(
        new DeleteTask('somewhereelse.com', QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });
});
