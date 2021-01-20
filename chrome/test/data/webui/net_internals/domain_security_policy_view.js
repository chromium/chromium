// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

GEN('#include "content/public/test/browser_test.h"');

// Anonymous namespace
(function() {

/*
 * Possible results of an HSTS query.
 * @enum {number}
 */
var QueryResultType = {SUCCESS: 0, NOT_FOUND: 1, ERROR: 2};

/**
 * A Task that waits for the results of a lookup query. Once the results are
 * received, checks them before completing.  Does not initiate the query.
 * @param {string} domain The domain that was looked up.
 * @param {string} inputId The ID of the input element for the lookup domain.
 * @param {string} outputId The ID of the element where the results are
       presented.
 * @param {QueryResultType} queryResultType The expected result type of the
 *     results of the query.
 * @extends {NetInternalsTest.Task}
 */
function CheckQueryResultTask(domain, inputId, outputId, queryResultType) {
  this.domain_ = domain;
  this.inputId_ = inputId;
  this.outputId_ = outputId;
  this.queryResultType_ = queryResultType;
  NetInternalsTest.Task.call(this);
}

CheckQueryResultTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Validates |result| and completes the task.
   * @param {object} result Results from the query.
   */
  onQueryResult_: function(result) {
    // Ignore results after |this| is finished.
    if (this.isDone()) {
      return;
    }

    expectEquals(this.domain_, $(this.inputId_).value);

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
  },

  /**
   * On errors, checks the result.
   * @param {object} result Results from the query.
   */
  checkError_: function(result) {
    expectEquals(QueryResultType.ERROR, this.queryResultType_);
    expectEquals(result.error, $(this.outputId_).innerText);
  },

  /**
   * Checks the result when the entry was not found.
   * @param {object} result Results from the query.
   */
  checkNotFound_: function(result) {
    expectEquals(QueryResultType.NOT_FOUND, this.queryResultType_);
    expectEquals('Not found', $(this.outputId_).innerText);
  },

  /**
   * Checks successful results.
   * @param {object} result Results from the query.
   */
  checkSuccess_: function(result) {
    expectEquals(QueryResultType.SUCCESS, this.queryResultType_);
    // Verify that the domain appears somewhere in the displayed text.
    var outputText = $(this.outputId_).innerText;
    expectLE(0, outputText.search(this.domain_));
  }
};

/**
 * A Task that waits for the results of an HSTS query. Once the results are
 * received, checks them before completing. Does not initiate the query.
 * @param {string} domain The domain expected in the returned results.
 * @param {bool} stsSubdomains Whether or not the stsSubdomains flag is expected
 *     to be set in the returned results.  Ignored on error and not found
 *     results.
 * @param {QueryResultType} queryResultType The expected result type of the
 *     results of the query.
 * @extends {CheckQueryResultTask}
 */
function CheckHSTSQueryResultTask(domain, stsSubdomains, queryResultType) {
  this.stsSubdomains_ = stsSubdomains;
  CheckQueryResultTask.call(
      this, domain, DomainSecurityPolicyView.QUERY_HSTS_INPUT_ID,
      DomainSecurityPolicyView.QUERY_HSTS_OUTPUT_DIV_ID, queryResultType);
}

CheckHSTSQueryResultTask.prototype = {
  __proto__: CheckQueryResultTask.prototype,

  /**
   * Starts watching for the query results.
   */
  start: function() {
    DomainSecurityPolicyView.getInstance().addHSTSObserverForTest(this);
  },

  /**
   * Callback from the BrowserBridge.  Validates |result| and completes the
   * task.
   * @param {object} result Results from the query.
   */
  onHSTSQueryResult: function(result) {
    this.onQueryResult_(result);
  },

  /**
   * Checks successful results.
   * @param {object} result Results from the query.
   */
  checkSuccess_: function(result) {
    expectEquals(this.stsSubdomains_, result.dynamic_sts_include_subdomains);
    CheckQueryResultTask.prototype.checkSuccess_.call(this, result);
  }
};

/**
 * A Task to try and add an HSTS domain via the HTML form. The task will wait
 * until the results from the automatically sent query have been received, and
 * then checks them against the expected values.
 * @param {string} domain The domain to send and expected to be returned.
 * @param {bool} stsSubdomains Whether the HSTS subdomain checkbox should be
 *     selected. Also the corresponding expected return value, in the success
 *     case.
 * @param {QueryResultType} queryResultType Expected result type.
 * @extends {CheckHSTSQueryResultTask}
 */
function AddHSTSTask(domain, stsSubdomains, queryResultType) {
  CheckHSTSQueryResultTask.call(this, domain, stsSubdomains, queryResultType);
}

AddHSTSTask.prototype = {
  __proto__: CheckHSTSQueryResultTask.prototype,

  /**
   * Fills out the add form, simulates a click to submit it, and starts
   * listening for the results of the query that is automatically submitted.
   */
  start: function() {
    $(DomainSecurityPolicyView.ADD_HSTS_INPUT_ID).value = this.domain_;
    $(DomainSecurityPolicyView.ADD_STS_CHECK_ID).checked = this.stsSubdomains_;
    $(DomainSecurityPolicyView.ADD_HSTS_SUBMIT_ID).click();
    CheckHSTSQueryResultTask.prototype.start.call(this);
  }
};

/**
 * A Task to query a domain and wait for the results. Parameters mirror those of
 * CheckHSTSQueryResultTask, except |domain| is also the name of the domain to
 * query.
 * @extends {CheckHSTSQueryResultTask}
 */
function QueryHSTSTask(domain, stsSubdomains, queryResultType) {
  CheckHSTSQueryResultTask.call(this, domain, stsSubdomains, queryResultType);
}

QueryHSTSTask.prototype = {
  __proto__: CheckHSTSQueryResultTask.prototype,

  /**
   * Fills out the query form, simulates a click to submit it, and starts
   * listening for the results.
   */
  start: function() {
    CheckHSTSQueryResultTask.prototype.start.call(this);
    $(DomainSecurityPolicyView.QUERY_HSTS_INPUT_ID).value = this.domain_;
    $(DomainSecurityPolicyView.QUERY_HSTS_SUBMIT_ID).click();
  }
};

/**
 * Task that deletes a single domain, then queries the deleted domain to make
 * sure it's gone.
 * @param {string} domain The domain to delete.
 * @param {QueryResultType} queryResultType The result of the query.  Can be
 *     QueryResultType.ERROR or QueryResultType.NOT_FOUND.
 * @extends {QueryTask}
 */
function DeleteTask(domain, queryResultType) {
  expectNotEquals(queryResultType, QueryResultType.SUCCESS);
  this.domain_ = domain;
  QueryHSTSTask.call(this, domain, false, queryResultType);
}

DeleteTask.prototype = {
  __proto__: QueryHSTSTask.prototype,

  /**
   * Fills out the delete form and simulates a click to submit it.  Then sends
   * a query.
   */
  start: function() {
    $(DomainSecurityPolicyView.DELETE_INPUT_ID).value = this.domain_;
    $(DomainSecurityPolicyView.DELETE_SUBMIT_ID).click();
    QueryHSTSTask.prototype.start.call(this);
  }
};

/**
 * A Task that waits for the results of an Expect-CT query. Once the results are
 * received, checks them before completing.  Does not initiate the query.
 * @param {string} domain The domain expected in the returned results.
 * @param {bool} enforce Whether or not the 'enforce' flag is expected
 *     to be set in the returned results.  Ignored on error and not found
 *     results.
 * @param {string} reportUri Expected report URI for the policy. Ignored on
 *     error and not found results.
 * @param {QueryResultType} queryResultType The expected result type of the
 *     results of the query.
 * @extends {CheckQueryResultTask}
 */
function CheckExpectCTQueryResultTask(
    domain, enforce, reportUri, queryResultType) {
  this.enforce_ = enforce;
  this.reportUri_ = reportUri;
  CheckQueryResultTask.call(
      this, domain, DomainSecurityPolicyView.QUERY_EXPECT_CT_INPUT_ID,
      DomainSecurityPolicyView.QUERY_EXPECT_CT_OUTPUT_DIV_ID, queryResultType);
}

CheckExpectCTQueryResultTask.prototype = {
  __proto__: CheckQueryResultTask.prototype,

  /**
   * Starts watching for the query results.
   */
  start: function() {
    DomainSecurityPolicyView.getInstance().addExpectCTObserverForTest(this);
  },

  /**
   * Callback from the BrowserBridge.  Validates |result| and completes the
   * task.
   * @param {object} result Results from the query.
   */
  onExpectCTQueryResult: function(result) {
    this.onQueryResult_(result);
  },

  /**
   * Checks successful results.
   * @param {object} result Results from the query.
   */
  checkSuccess_: function(result) {
    expectEquals(this.enforce_, result.dynamic_expect_ct_enforce);
    expectEquals(this.reportUri_, result.dynamic_expect_ct_report_uri);
    CheckQueryResultTask.prototype.checkSuccess_.call(this, result);
  }
};

/**
 * A Task to try and add an Expect-CT domain via the HTML form. The task will
 * wait until the results from the automatically sent query have been received,
 * and then checks them against the expected values.
 * @param {string} domain The domain to send and expected to be returned.
 * @param {bool} enforce Whether the enforce checkbox should be
 *     selected. Also the corresponding expected return value, in the success
 *     case.
 * @param {string} reportUri The report URI for the Expect-CT policy. Also the
 *     corresponding expected return value, on success.
 * @param {QueryResultType} queryResultType Expected result type.
 * @extends {CheckExpectCTQueryResultTask}
 */
function AddExpectCTTask(domain, enforce, reportUri, queryResultType) {
  CheckExpectCTQueryResultTask.call(
      this, domain, enforce, reportUri, queryResultType);
}

AddExpectCTTask.prototype = {
  __proto__: CheckExpectCTQueryResultTask.prototype,

  /**
   * Fills out the add form, simulates a click to submit it, and starts
   * listening for the results of the query that is automatically submitted.
   */
  start: function() {
    $(DomainSecurityPolicyView.ADD_EXPECT_CT_INPUT_ID).value = this.domain_;
    $(DomainSecurityPolicyView.ADD_EXPECT_CT_ENFORCE_CHECK_ID).checked =
        this.enforce_;
    $(DomainSecurityPolicyView.ADD_EXPECT_CT_REPORT_URI_INPUT_ID).value =
        this.reportUri_;
    $(DomainSecurityPolicyView.ADD_EXPECT_CT_SUBMIT_ID).click();
    CheckExpectCTQueryResultTask.prototype.start.call(this);
  }
};

/**
 * A Task to query a domain and wait for the results.  Parameters mirror those
 * of CheckExpectCTQueryResultTask, except |domain| is also the name of the
 * domain to query.
 * @extends {CheckExpectCTQueryResultTask}
 */
function QueryExpectCTTask(domain, enforce, reportUri, queryResultType) {
  CheckExpectCTQueryResultTask.call(
      this, domain, enforce, reportUri, queryResultType);
}

QueryExpectCTTask.prototype = {
  __proto__: CheckExpectCTQueryResultTask.prototype,

  /**
   * Fills out the query form, simulates a click to submit it, and starts
   * listening for the results.
   */
  start: function() {
    CheckExpectCTQueryResultTask.prototype.start.call(this);
    $(DomainSecurityPolicyView.QUERY_EXPECT_CT_INPUT_ID).value = this.domain_;
    $(DomainSecurityPolicyView.QUERY_EXPECT_CT_SUBMIT_ID).click();
  }
};

/**
 * A Task to retrieve a test report-uri.
 * @extends {NetInternalsTest.Task}
 */
function GetTestReportURITask() {
  NetInternalsTest.Task.call(this);
}

GetTestReportURITask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Sets |NetInternals.callback|, and sends the request to the browser process.
   */
  start: function() {
    NetInternalsTest.setCallback(this.onReportURIReceived_.bind(this));
    chrome.send('setUpTestReportURI');
  },

  /**
   * Saves the received report-uri and completes the task.
   * @param {string} reportURI Report URI received from the browser process.
   */
  onReportURIReceived_: function(reportURI) {
    this.reportURI_ = reportURI;
    this.onTaskDone();
  },

  /**
   * Returns the saved report-uri received from the browser process.
   */
  reportURI: function() {
    return this.reportURI_;
  }
};

/**
 * A Task to send a test Expect-CT report and wait for the result.
 * @param {getTestReportURITask} GetTestReportURITask The task that retrieved a
                                 test report-uri.
 * @extends {NetInternalsTest.Task}
 */
function SendTestReportTask(getTestReportURITask) {
  this.reportURITask_ = getTestReportURITask;
  NetInternalsTest.Task.call(this);
}

SendTestReportTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Sends the test report and starts watching for the result.
   */
  start: function() {
    DomainSecurityPolicyView.getInstance().addExpectCTObserverForTest(this);
    $(DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_INPUT_ID).value =
        this.reportURITask_.reportURI();
    $(DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_SUBMIT_ID).click();
  },

  /**
   * Callback from the BrowserBridge.  Checks that |result| indicates success
   * and completes the task.
   * @param {object} result Results from the query.
   */
  onExpectCTTestReportResult: function(result) {
    expectEquals('success', result);
    // Start the next task asynchronously, so it can add another observer
    // without getting the current result.
    window.setTimeout(this.onTaskDone.bind(this), 1);
  },
};

/**
 * Checks that querying a domain that was never added fails.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewQueryNotFound',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(
          new QueryHSTSTask('somewhere.com', false, QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Checks that querying a domain with an invalid name returns an error.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewQueryError',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(
          new QueryHSTSTask('\u3024', false, QueryResultType.ERROR));
      taskQueue.run();
    });

/**
 * Deletes a domain that was never added.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewDeleteNotFound',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(
          new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Deletes a domain that returns an error on lookup.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewDeleteError',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(new DeleteTask('\u3024', QueryResultType.ERROR));
      taskQueue.run();
    });

/**
 * Adds a domain and then deletes it.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewAddDelete',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(
          new AddHSTSTask('somewhere.com', false, QueryResultType.SUCCESS));
      taskQueue.addTask(
          new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Tries to add a domain with an invalid name.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewAddFail',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(new AddHSTSTask(
          '0123456789012345678901234567890' +
              '012345678901234567890123456789012345',
          false, QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Tries to add a domain with a name that errors out on lookup due to having
 * non-ASCII characters in it.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewAddError',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(
          new AddHSTSTask('\u3024', false, QueryResultType.ERROR));
      taskQueue.run();
    });

/**
 * Adds the same domain twice in a row, modifying some values the second time.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewAddOverwrite',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(
          new AddHSTSTask('somewhere.com', true, QueryResultType.SUCCESS));
      taskQueue.addTask(
          new AddHSTSTask('somewhere.com', false, QueryResultType.SUCCESS));
      taskQueue.addTask(
          new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Adds two different domains and then deletes them.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewAddTwice',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
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
      taskQueue.addTask(new QueryHSTSTask(
          'somewhereelse.com', true, QueryResultType.SUCCESS));
      taskQueue.addTask(
          new DeleteTask('somewhereelse.com', QueryResultType.NOT_FOUND));
      taskQueue.run(true);
    });

/**
 * Checks that querying an Expect-CT domain that was never added fails.
 */
TEST_F(
    'NetInternalsTest',
    'netInternalsDomainSecurityPolicyViewExpectCTQueryNotFound', function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(new QueryExpectCTTask(
          'somewhere.com', false, '', QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Checks that querying an Expect-CT domain with an invalid name returns an
 * error.
 */
TEST_F(
    'NetInternalsTest',
    'netInternalsDomainSecurityPolicyViewExpectCTQueryError', function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(
          new QueryExpectCTTask('\u3024', false, '', QueryResultType.ERROR));
      taskQueue.run();
    });

/**
 * Adds an Expect-CT domain and then deletes it.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewExpectCTAddDelete',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(new AddExpectCTTask(
          'somewhere.com', true, '', QueryResultType.SUCCESS));
      taskQueue.addTask(
          new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Tries to add an Expect-CT domain with an invalid name.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewExpectCTAddFail',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(new AddExpectCTTask(
          '0123456789012345678901234567890' +
              '012345678901234567890123456789012345',
          false, '', QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Tries to add an Expect-CT domain with a name that errors out on lookup due to
 * having non-ASCII characters in it.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewExpectCTAddError',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(
          new AddExpectCTTask('\u3024', false, '', QueryResultType.ERROR));
      taskQueue.run();
    });

/**
 * Adds the same Expect-CT domain twice in a row, modifying some values the
 * second time.
 */
TEST_F(
    'NetInternalsTest',
    'netInternalsDomainSecurityPolicyViewExpectCTAddOverwrite', function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      taskQueue.addTask(new AddExpectCTTask(
          'somewhere.com', true, 'https://reporting.test/',
          QueryResultType.SUCCESS));
      taskQueue.addTask(new AddExpectCTTask(
          'somewhere.com', false, 'https://other-reporting.test/',
          QueryResultType.SUCCESS));
      taskQueue.addTask(
          new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
      taskQueue.run();
    });

/**
 * Adds two different Expect-CT domains and then deletes them.
 */
TEST_F(
    'NetInternalsTest', 'netInternalsDomainSecurityPolicyViewExpectCTAddTwice',
    function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
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
      taskQueue.run(true);
    });


/**
 * Checks that sending an Expect-CT test report succeeds.
 */
TEST_F(
    'NetInternalsTest',
    'netInternalsDomainSecurityPolicyViewExpectCTTestReport', function() {
      NetInternalsTest.switchToView('hsts');
      taskQueue = new NetInternalsTest.TaskQueue(true);
      var getReportURITask = new GetTestReportURITask();
      taskQueue.addTask(getReportURITask);
      taskQueue.addTask(new SendTestReportTask(getReportURITask));
      taskQueue.run();
    });

})();  // Anonymous namespace
