// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The identation for generated Python code.
 *
 * @type {string}
 */
const IDENT_PAD = '  ';

/**
 * If the extension should generate Python code for signup or change password
 * forms, then this value should be 'True' (i.e. it will be expected that
 * password generation will be offered on the given field). Otherwise, the value
 * should be 'False'.
 *
 * @type {string}
 */
const IS_PWD_CREATION_VALUE = 'False';

/**
 * Used by the extension to store the steps that user performs to reach
 * a password form (signin or signup).
 *
 * When the user is reaching the form, the extension saves to the array the
 * URLs of visited pages, clicked elements' CSS selectors and other stuff.
 * When the user has reached the form, the extension converts the array to
 * Python test code and clears the array (before visiting the next site).
 *
 * @type {Array}
 */
let steps = [];

/**
 * The index of the last visited site from |sitesToVisit| (sites_to_visit.js).
 *
 * @type {number}
 */
let lastVisitedSiteIndex = 0;

/**
 * Generated Python tests.
 *
 * A test is printed to the background page's console right after the user click
 * on the password field. The test is also appended to this variable to be able
 * to print all generated tests at once.
 * Attention: if the extension is reloaded, the variable is cleared. So, print
 * the variable and save the generated tests before extension reload,
 * browser closing or so.
 *
 * @type {string}
 */
let allTests = '\n';

/**
 * Return the name of the test based on the form's url |url|
 * (e.g. http://login.example.com/path => test_login_example_com).
 *
 * @param {string} url The form's url.
 * @return {string} The test name.
 */
function getTestName(url) {
  const a = document.createElement('a');
  a.href = url;
  return 'test_' + a.hostname.split(/[.-]+/).join('_');
}

/**
 * Removes query and anchor from |url|
 * (e.g. https://example.com/path?query=1#anchor => https://example.com/path).
 *
 * @param {string} url The full url to be processed.
 * @return {string} The url w/o parameters and anchors.
 */
function stripUrl(url) {
  const a = document.createElement('a');
  a.href = url;
  return a.origin + a.pathname;
}

/**
 * Removes the elements before the first element in |steps| that might be the
 * first in Python script (i.e. it is possible to go to its url and find needed
 * element). See |couldBeFirstPageOfScript| on content.js for clarification.
 *
 * @param {Array} steps The steps to reach the password field.
 * @return {Array} Reduced list of steps.
 */
function removeUnnecessarySteps(steps) {
  const n = steps.length;
  const result = [];
  for (let i = n - 1; i >= 0; i--) {
    result.unshift(steps[i]);
    if (steps[i].couldBeFirst) {
      break;
    }
  }
  return result;
}

/**
 * Generate python code to switch to another frame.
 *
 * @param {Object} step A step of the script.
 * @return {string} Python code that performs frame switching.
 */
function switchToIframeIfNecessary(step) {
  let result = '';
  for (let i = 0; i < step.frames.length; i++) {
     result += IDENT_PAD + IDENT_PAD + 'self.SwitchTo(\'' + step.frames[i] +
         '\')\n';
  }
  return result;
}

/**
 * Outputs to the console the code of a Python test based on script steps
 * accumulated in |steps|. Also appends the test code to |allTests|.
 */
function outputPythonTestCode() {
  const lastStepUrl = stripUrl(steps[steps.length - 1].url);
  let test = '';
  test += IDENT_PAD + 'def ' + getTestName(steps[steps.length - 1].url) +
      '(self):\n';
  steps = removeUnnecessarySteps(steps);
  test += IDENT_PAD + IDENT_PAD + 'self.GoTo("' + stripUrl(steps[0].url) +
      '")\n';
  for (let i = 0; i <= steps.length - 2; i++) {
    test += switchToIframeIfNecessary(steps[i]);
    test += IDENT_PAD + IDENT_PAD + 'self.Click("' + steps[i].selector + '")\n';
  }
  test += switchToIframeIfNecessary(steps[steps.length - 1]);

  test +=
      IDENT_PAD + IDENT_PAD + 'self.CheckPwdField("' +
      steps[steps.length - 1].selector + '", ' +
      'is_pwd_creation=' + IS_PWD_CREATION_VALUE + ')\n';
  test += '\n';

  console.info(test);
  allTests += test;
  steps = [];
}

/**
 * Moves the current tab to the next site.
 */
function visitNextSite() {
  console.info(
      'next site: ' + sitesToVisit[lastVisitedSiteIndex] + ' ' +
      lastVisitedSiteIndex);
  chrome.tabs.update({url: 'http://' + sitesToVisit[lastVisitedSiteIndex]});
  steps = [];
  lastVisitedSiteIndex += 1;
}

/**
 * Add listener for messages from content script.
 */
chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
    steps.push(request);
    // If the user clicked on the password field, output a Python test and go to
    // the next site.
    if (request.isPwdField) {
      outputPythonTestCode();
      visitNextSite();
    }
});

/**
 * Add listener for browser action (i.e. click on the icon of the extension).
 */
chrome.browserAction.onClicked.addListener(function(tab) {
  visitNextSite();
});
