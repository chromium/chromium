// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DomainSecurityPolicyView} from 'chrome://net-internals/domain_security_policy_view.js';
import {$} from 'chrome://resources/js/util.m.js';

import {assertEquals, assertLE, assertNotEquals} from '../chai_assert.js';

import {Task, TaskQueue} from './task_queue.js';
import {switchToView} from './test_util.js';

window.domain_security_policy_view_test = {};
const domain_security_policy_view_test =
    window.domain_security_policy_view_test;
domain_security_policy_view_test.suiteName = 'DomainSecurityPolicyViewTest';
/** @enum {string} */
domain_security_policy_view_test.TestNames = {
  QueryNotFound: 'query not found',
  QueryError: 'query error',
  DeleteNotFound: 'delete not found',
  DeleteError: 'delete error',
  AddDelete: 'add delete',
  AddFail: 'add fail',
  AddError: 'add error',
  AddOverwrite: 'add overwrite',
  AddTwice: 'add twice',
  ExpectCTQueryNotFound: 'expect ct query not found',
  ExpectCTQueryError: 'expect ct query error',
  ExpectCTAddDelete: 'expect ct add delete',
  ExpectCTAddFail: 'expect ct add fail',
  ExpectCTAddError: 'expect ct add error',
  ExpectCTAddOverwrite: 'expect ct add overwrite',
  ExpectCTAddTwice: 'expect ct add twice',
  ExpectCTTestReport: 'expect ct test report',
};

suite(domain_security_policy_view_test.suiteName, function() {
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
   * A Task that waits for the results of an Expect-CT query. Once the results
   * are received, checks them before completing.  Does not initiate the query.
   */
  class CheckExpectCTQueryResultTask extends CheckQueryResultTask {
    /**
     * @param {string} domain The domain expected in the returned results.
     * @param {bool} enforce Whether or not the 'enforce' flag is expected
     *     to be set in the returned results.  Ignored on error and not found
     *     results.
     * @param {string} reportUri Expected report URI for the policy. Ignored on
     *     error and not found results.
     * @param {QueryResultType} queryResultType The expected result type of the
     *     results of the query.
     */
    constructor(domain, enforce, reportUri, queryResultType) {
      super(
          domain, DomainSecurityPolicyView.QUERY_EXPECT_CT_INPUT_ID,
          DomainSecurityPolicyView.QUERY_EXPECT_CT_OUTPUT_DIV_ID,
          queryResultType);
      this.enforce_ = enforce;
      this.reportUri_ = reportUri;
    }

    /**
     * Starts watching for the query results.
     */
    start() {
      DomainSecurityPolicyView.getInstance().addExpectCTObserverForTest(this);
    }

    /**
     * Callback from the BrowserBridge.  Validates |result| and completes the
     * task.
     * @param {object} result Results from the query.
     */
    onExpectCTQueryResult(result) {
      this.onQueryResult_(result);
    }

    /**
     * Checks successful results.
     * @param {object} result Results from the query.
     */
    checkSuccess_(result) {
      assertEquals(this.enforce_, result.dynamic_expect_ct_enforce);
      assertEquals(this.reportUri_, result.dynamic_expect_ct_report_uri);
      super.checkSuccess_(result);
    }
  }

  /**
   * A Task to try and add an Expect-CT domain via the HTML form. The task will
   * wait until the results from the automatically sent query have been
   * received, and then checks them against the expected values.
   */
  class AddExpectCTTask extends CheckExpectCTQueryResultTask {
    /**
     * Fills out the add form, simulates a click to submit it, and starts
     * listening for the results of the query that is automatically submitted.
     */
    start() {
      super.start();
      $(DomainSecurityPolicyView.ADD_EXPECT_CT_INPUT_ID).value = this.domain_;
      $(DomainSecurityPolicyView.ADD_EXPECT_CT_ENFORCE_CHECK_ID).checked =
          this.enforce_;
      $(DomainSecurityPolicyView.ADD_EXPECT_CT_REPORT_URI_INPUT_ID).value =
          this.reportUri_;
      $(DomainSecurityPolicyView.ADD_EXPECT_CT_SUBMIT_ID).click();
    }
  }

  /**
   * A Task to query a domain and wait for the results.  Parameters mirror those
   * of CheckExpectCTQueryResultTask, except |domain| is also the name of the
   * domain to query.
   */
  class QueryExpectCTTask extends CheckExpectCTQueryResultTask {
    /**
     * Fills out the query form, simulates a click to submit it, and starts
     * listening for the results.
     */
    start() {
      super.start();
      $(DomainSecurityPolicyView.QUERY_EXPECT_CT_INPUT_ID).value = this.domain_;
      $(DomainSecurityPolicyView.QUERY_EXPECT_CT_SUBMIT_ID).click();
    }
  }

  /**
   * A Task to retrieve a test report-uri.
   */
  class GetTestReportURITask extends Task {
    /**
     * Sets |NetInternals.callback|, and sends the request to the browser
     * process.
     */
    start() {
      NetInternalsTest.setCallback(this.onReportURIReceived_.bind(this));
      chrome.send('setUpTestReportURI');
    }

    /**
     * Saves the received report-uri and completes the task.
     * @param {string} reportURI Report URI received from the browser process.
     */
    onReportURIReceived_(reportURI) {
      this.reportURI_ = reportURI;
      this.onTaskDone();
    }

    /**
     * Returns the saved report-uri received from the browser process.
     */
    reportURI() {
      return this.reportURI_;
    }
  }

  /**
   * A Task to send a test Expect-CT report and wait for the result.
   */
  class SendTestReportTask extends Task {
    /**
     * @param {getTestReportURITask} GetTestReportURITask The task that
     *     retrieved a test report-uri.
     */
    constructor(getTestReportURITask) {
      super();
      this.reportURITask_ = getTestReportURITask;
    }

    /**
     * Sends the test report and starts watching for the result.
     */
    start() {
      DomainSecurityPolicyView.getInstance().addExpectCTObserverForTest(this);
      $(DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_INPUT_ID).value =
          this.reportURITask_.reportURI();
      $(DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_SUBMIT_ID).click();
    }

    /**
     * Callback from the BrowserBridge.  Checks that |result| indicates success
     * and completes the task.
     * @param {object} result Results from the query.
     */
    onExpectCTTestReportResult(result) {
      assertEquals('success', result);
      // Start the next task asynchronously, so it can add another observer
      // without getting the current result.
      window.setTimeout(this.onTaskDone.bind(this), 1);
    }
  }

  /**
   * Checks that querying a domain that was never added fails.
   */
  test(domain_security_policy_view_test.TestNames.QueryNotFound, function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new QueryHSTSTask('somewhere.com', false, QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Checks that querying a domain with an invalid name returns an error.
   */
  test(domain_security_policy_view_test.TestNames.QueryError, function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new QueryHSTSTask('\u3024', false, QueryResultType.ERROR));
    return taskQueue.run();
  });

  /**
   * Deletes a domain that was never added.
   */
  test(domain_security_policy_view_test.TestNames.DeleteNotFound, function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Deletes a domain that returns an error on lookup.
   */
  test(domain_security_policy_view_test.TestNames.DeleteError, function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new DeleteTask('\u3024', QueryResultType.ERROR));
    taskQueue.run();
  });

  /**
   * Adds a domain and then deletes it.
   */
  test(domain_security_policy_view_test.TestNames.AddDelete, function() {
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
  test(domain_security_policy_view_test.TestNames.AddFail, function() {
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
  test(domain_security_policy_view_test.TestNames.AddError, function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new AddHSTSTask('\u3024', false, QueryResultType.ERROR));
    taskQueue.run();
  });

  /**
   * Adds the same domain twice in a row, modifying some values the second time.
   */
  test(domain_security_policy_view_test.TestNames.AddOverwrite, function() {
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
  test(domain_security_policy_view_test.TestNames.AddTwice, function() {
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

  /**
   * Checks that querying an Expect-CT domain that was never added fails.
   */
  test(
      domain_security_policy_view_test.TestNames.ExpectCTQueryNotFound,
      function() {
        switchToView('hsts');
        const taskQueue = new TaskQueue(true);
        taskQueue.addTask(new QueryExpectCTTask(
            'somewhere.com', false, '', QueryResultType.NOT_FOUND));
        return taskQueue.run();
      });

  /**
   * Checks that querying an Expect-CT domain with an invalid name returns an
   * error.
   */
  test(
      domain_security_policy_view_test.TestNames.ExpectCTQueryError,
      function() {
        switchToView('hsts');
        const taskQueue = new TaskQueue(true);
        taskQueue.addTask(
            new QueryExpectCTTask('\u3024', false, '', QueryResultType.ERROR));
        return taskQueue.run();
      });

  /**
   * Adds an Expect-CT domain and then deletes it.
   */
  test(
      domain_security_policy_view_test.TestNames.ExpectCTAddDelete, function() {
        switchToView('hsts');
        const taskQueue = new TaskQueue(true);
        taskQueue.addTask(new AddExpectCTTask(
            'somewhere.com', true, '', QueryResultType.SUCCESS));
        taskQueue.addTask(
            new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
        return taskQueue.run();
      });

  /**
   * Tries to add an Expect-CT domain with an invalid name.
   */
  test(domain_security_policy_view_test.TestNames.ExpectCTAddFail, function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new AddExpectCTTask(
        '0123456789012345678901234567890' +
            '012345678901234567890123456789012345',
        false, '', QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Tries to add an Expect-CT domain with a name that errors out on lookup due
   * to having non-ASCII characters in it.
   */
  test(domain_security_policy_view_test.TestNames.ExpectCTAddError, function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(
        new AddExpectCTTask('\u3024', false, '', QueryResultType.ERROR));
    return taskQueue.run();
  });

  /**
   * Adds the same Expect-CT domain twice in a row, modifying some values the
   * second time.
   */
  test(
      domain_security_policy_view_test.TestNames.ExpectCTAddOverwrite,
      function() {
        switchToView('hsts');
        const taskQueue = new TaskQueue(true);
        taskQueue.addTask(new AddExpectCTTask(
            'somewhere.com', true, 'https://reporting.test/',
            QueryResultType.SUCCESS));
        taskQueue.addTask(new AddExpectCTTask(
            'somewhere.com', false, 'https://other-reporting.test/',
            QueryResultType.SUCCESS));
        taskQueue.addTask(
            new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
        return taskQueue.run();
      });

  /**
   * Adds two different Expect-CT domains and then deletes them.
   */
  test(domain_security_policy_view_test.TestNames.ExpectCTAddTwice, function() {
    switchToView('hsts');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new AddExpectCTTask(
        'somewhere.com', true, '', QueryResultType.SUCCESS));
    taskQueue.addTask(new QueryExpectCTTask(
        'somewhereelse.com', false, '', QueryResultType.NOT_FOUND));
    taskQueue.addTask(new AddExpectCTTask(
        'somewhereelse.com', true, 'https://reporting.test/',
        QueryResultType.SUCCESS));
    taskQueue.addTask(new QueryExpectCTTask(
        'somewhere.com', true, '', QueryResultType.SUCCESS));
    taskQueue.addTask(
        new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
    taskQueue.addTask(new QueryExpectCTTask(
        'somewhereelse.com', true, 'https://reporting.test/',
        QueryResultType.SUCCESS));
    taskQueue.addTask(
        new DeleteTask('somewhereelse.com', QueryResultType.NOT_FOUND));
    return taskQueue.run();
  });

  /**
   * Checks that sending an Expect-CT test report succeeds.
   */
  test(
      domain_security_policy_view_test.TestNames.ExpectCTTestReport,
      function() {
        switchToView('hsts');
        const taskQueue = new TaskQueue(true);
        const getReportURITask = new GetTestReportURITask();
        taskQueue.addTask(getReportURITask);
        taskQueue.addTask(new SendTestReportTask(getReportURITask));
        return taskQueue.run();
      });
});
