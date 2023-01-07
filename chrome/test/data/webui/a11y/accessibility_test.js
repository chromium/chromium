// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Accessibility Test API */

GEN_INCLUDE([
  'accessibility_audit_rules.js',
  '//third_party/axe-core/axe.js',
]);

/**
 * Accessibility Test
 * @namespace
 */
var AccessibilityTest = AccessibilityTest || {};

/**
 * @typedef {{
 *   runOnly: {
 *     type: string,
 *     values: Array<string>
 *   }
 * }}
 * @see https://github.com/dequelabs/axe-core/blob/develop/doc/API.md#options-parameter
 */
AccessibilityTest.AxeOptions;

/**
 * The violation filter object maps individual audit rule IDs to functions that
 * return true for elements to filter from that rule's violations.
 * @typedef {Object<string, function(!axe.NodeResult): boolean>}
 */
AccessibilityTest.ViolationFilter;

/**
 * @typedef {{
 *   name: string,
 *   axeOptions: ?AccessibilityTest.AxeOptions,
 *   setup: ?function,
 *   tests: Object<string, function(): ?Promise>,
 *   violationFilter: ?AccessibilityTest.ViolationFilter
 * }}
 */
AccessibilityTest.Definition;

/**
 * Run aXe-core accessibility audit, print console-friendly representation
 * of violations to console, and fail the test.
 * @param {!AccessibilityTest.Definition} testDef Object configuring the audit.
 * @return {Promise} A promise that will be resolved with the accessibility
 *    audit is complete.
 */
AccessibilityTest.runAudit_ = function(testDef) {
  // Ignore iron-iconset-svg elements that have duplicate ids and result in
  // false postives from the audit.
  const context = {exclude: ['iron-iconset-svg']};
  const options = testDef.axeOptions || {};
  // Element references needed for filtering audit results.
  options.elementRef = true;

  return new Promise((resolve, reject) => {
    axe.run(context, options, (err, results) => {
      if (err) {
        reject(err);
      }

      const filteredViolations = AccessibilityTest.filterViolations_(
          results.violations, testDef.violationFilter || {});

      const violationCount = filteredViolations.length;
      if (violationCount) {
        AccessibilityTest.print_(filteredViolations);
        reject('Found ' + violationCount + ' accessibility violations.');
      } else {
        resolve();
      }
    });
  });
};

/*
 * Get list of filtered audit violations.
 * @param {!Array<axe.Result>} violations List of accessibility violations.
 * @param {!AccessibilityTest.ViolationFilter} filter Object specifying set of
 *    violations to filter from the results.
 * @return {!Array<axe.Result>} List of filtered violations.
 */
AccessibilityTest.filterViolations_ = function(violations, filter) {
  if (Object.keys(filter).length === 0) {
    return violations;
  }

  const filteredViolations = [];
  // Check for and remove any nodes specified by filter.
  for (const violation of violations) {
    if (violation.id in filter) {
      const exclusionRule = filter[violation.id];
      violation.nodes = violation.nodes.filter((node) => !exclusionRule(node));
    }

    if (violation.nodes.length > 0) {
      filteredViolations.push(violation);
    }
  }
  return filteredViolations;
};

/**
 * Define a GTest test for each audit rule.
 * @param {string} testFixture Name of test fixture associated with the test.
 * @param {AccessibilityTestDefinition} testDef Object configuring the test.
 * @constructor
 */
AccessibilityTest.define = function(testFixture, testDef) {
  // Disable in debug mode because of timeouts.
  GEN('#if defined(NDEBUG)');

  const axeOptions = testDef.axeOptions || {};
  testDef.setup = testDef.setup || (() => {});

  // Define a test for each audit rule separately.
  const rules = axeOptions.runOnly ? axeOptions.runOnly.values :
                                     AccessibilityTest.ruleIds;
  rules.forEach((ruleId) => {
    // Skip rules disabled in axeOptions.
    if (axeOptions.rules && ruleId in axeOptions.rules &&
        !axeOptions.rules[ruleId].enabled) {
      return;
    }

    const newTestDef = Object.assign({}, testDef);
    newTestDef.name += '_' + ruleId;
    // Replace hyphens, which break the build.
    newTestDef.name = newTestDef.name.replace(new RegExp('-', 'g'), '_');
    newTestDef.axeOptions = Object.assign({}, axeOptions);
    newTestDef.axeOptions.runOnly = {type: 'rule', values: [ruleId]};

    TEST_F(testFixture, newTestDef.name, () => {
      // Define the mocha tests
      suite(newTestDef.name, () => {
        setup(newTestDef.setup.bind(newTestDef));
        for (const testMember in newTestDef.tests) {
          test(
              testMember,
              AccessibilityTest.getMochaTest_(testMember, newTestDef));
        }
      });
      mocha.grep(newTestDef.name).run();
    });
  });

  GEN('#endif  // defined(NDEBUG)');
};


/**
 *
 * Return a function that runs the accessibility audit after executing
 * the function corresponding to the |testDef.tests.testMember|.
 * @param {string} testMember The name of the mocha test
 * @param {AccessibilityTestDefinition} testDef Object configuring the test
 *    suite to which this test belongs.
 */
AccessibilityTest.getMochaTest_ = function(testMember, testDef) {
  return () => {
    // Run commands specified by the test definition followed by the
    // accessibility audit.
    const promise = testDef.tests[testMember].call(testDef);
    if (promise) {
      return promise.then(() => AccessibilityTest.runAudit_(testDef));
    } else {
      return AccessibilityTest.runAudit_(testDef);
    }
  };
};

/**
 * Remove circular references in |violations| and print violations to the
 * console.
 * @param {!Array<axe.Result>} List of violations to display
 */
AccessibilityTest.print_ = function(violations) {
  // Elements have circular references and must be removed before printing.
  for (const violation of violations) {
    for (const node of violation.nodes) {
      delete node['element'];
      ['all', 'any', 'none'].forEach((attribute) => {
        for (const checkResult of node[attribute]) {
          for (const relatedNode of checkResult.relatedNodes) {
            delete relatedNode['element'];
          }
        }
      });
    }
  }
  // eslint-disable-next-line no-console
  console.log(JSON.stringify(violations, null, 4));
};
