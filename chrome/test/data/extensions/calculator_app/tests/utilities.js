/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The utility class defined in this file allow calculator tests to be written
 * more succinctly.
 *
 * Tests that would be written with QUnit like this:
 *
 *   test('Two Plus Two', function() {
 *     var mock = window.mockView.create();
 *     var controller = new Controller(new Model(8), mock);
 *     deepEqual(mock.testButton('2'), [null, null, '2'], '2');
 *     deepEqual(mock.testButton('+'), ['2', '+', null], '+');
 *     deepEqual(mock.testButton('2'), ['2', '+', '2'], '2');
 *     deepEqual(mock.testButton('='), ['4', '=', null], '=');
 *   });
 *
 * can instead be written as:
 *
 *   var run = calculatorTestRun.create();
 *   run.test('Two Plus Two', '2 + 2 = [4]');
 */

'use strict';

window.mockView = {

  create: function() {
    var view = Object.create(this);
    view.display = [];
    return view;
  },

  clearDisplay: function(values) {
    this.display = [];
    this.addValues(values);
  },

  addResults: function(values) {
    this.display.push([]);
    this.addValues(values);
  },

  addValues: function(values) {
    this.display.push([
      values.accumulator || '',
      values.operator || '',
      values.operand || ''
    ]);
  },

  setValues: function(values) {
    this.display.pop();
    this.addValues(values);
  },

  getValues: function() {
    var last = this.display[this.display.length - 1];
    return {
      accumulator: last && last[0] || null,
      operator: last && last[1] || null,
      operand: last && last[2] || null
    };
  },

  testButton: function(button) {
    this.onButton.call(this, button);
    return this.display;
  }

};

window.calculatorTestRun = {

  BUTTONS: {
    '0': 'zero',
    '1': 'one',
    '2': 'two',
    '3': 'three',
    '4': 'four',
    '5': 'five',
    '6': 'six',
    '7': 'seven',
    '8': 'eight',
    '9': 'nine',
    '.': 'point',
    '+': 'add',
    '-': 'subtract',
    '*': 'multiply',
    '/': 'divide',
    '=': 'equals',
    '~': 'negate',
    'A': 'clear',
    '<': 'back'
  },

  NAMES: {
    '~': '+ / -',
    'A': 'AC',
    '<': 'back',
  },

  /**
   * Returns an object representing a run of calculator tests.
   */
  create: function() {
    var run = Object.create(this);
    run.tests = [];
    run.success = true;
    return run;
  },

  /**
   * Runs a test defined as either a sequence or a function.
   */
  test: function(name, test) {
    this.tests.push({name: name, steps: [], success: true});
    if (typeof test === 'string')
      this.testSequence_(name, test);
    else if (typeof test === 'function')
      test.call(this, new Controller(new Model(8), window.mockView.create()));
    else
      this.fail(this.getDescription_('invalid test: ', test));
  },

  /**
   * Log test failures to the console.
   */
  log: function() {
    var parts = ['\n\n', 0, ' tests passed, ', 0, ' failed.\n\n'];
    if (!this.success) {
      this.tests.forEach(function(test, index) {
        var number = this.formatNumber_(index + 1, 2);
        var prefix = test.success ? 'PASS: ' : 'FAIL: ';
        parts[test.success ? 1 : 3] += 1;
        parts.push(number, ') ', prefix, test.name, '\n');
        test.steps.forEach(function(step) {
          var prefix = step.success ? 'PASS: ' : 'FAIL: ';
          step.messages.forEach(function(message) {
            parts.push('    ', prefix, message, '\n');
            prefix = '      ';
          });
        });
        parts.push('\n');
      }.bind(this));
      console.log(parts.join(''));
    }
  },

  /**
   * Verify that actual values after a test step match expectations.
   */
  verify: function(expected, actual, message) {
    if (this.areEqual_(expected, actual))
      this.succeed(message);
    else
      this.fail(message, expected, actual);
  },

  /**
   * Record a successful test step.
   */
  succeed: function(message) {
    var test = this.tests[this.tests.length - 1];
    test.steps.push({success: true, messages: [message]});
  },

  /**
   * Fail the current test step. Expected and actual values are optional.
   */
  fail: function(message, expected, actual) {
    var test = this.tests[this.tests.length - 1];
    var failure = {success: false, messages: [message]};
    if (expected !== undefined) {
      failure.messages.push(this.getDescription_('expected: ', expected));
      failure.messages.push(this.getDescription_('actual:   ', actual));
    }
    test.steps.push(failure);
    test.success = false;
    this.success = false;
  },

  /**
   * @private
   * Tests how a new calculator controller handles a sequence of numbers,
   * operations, and commands, verifying that the controller's view has expected
   * values displayed after each input handled by the controller.
   *
   * Within the sequence string, expected values must be specified as arrays of
   * the form described below. The strings '~', '<', and 'A' is interpreted as
   * the commands '+ / -', 'back', and 'AC' respectively, and other strings are
   * interpreted as the digits, periods, operations, and commands represented
   * by those strings.
   *
   * Expected values are sequences of arrays of the following forms:
   *
   *   []
   *   [accumulator]
   *   [accumulator operator operand]
   *   [accumulator operator prefix suffix]
   *
   * where |accumulator|, |operand|, |prefix|, and |suffix| are numbers or
   * underscores and |operator| is one of the operator characters or an
   * underscore. The |operand|, |prefix|, and |suffix| numbers may contain
   * leading zeros and embedded '=' characters which will be interpreted as
   * described in the comments for the |testNumber_()| method above. Underscores
   * represent values that are expected to be blank. '[]' arrays represent
   * horizontal separators expected in the display. '[accumulator]' arrays
   * adjust the last expected value array by setting only its accumulator value.
   * If that value is already set they behave like '[accumulator _ accumulator]'
   * arrays.
   *
   * Expected value array must be specified just after the sequence element
   * which they are meant to test, like this:
   *
   *   run.testSequence_(controller, '12 - 34 = [][-22 _ -22]')
   *
   * When expected values are not specified for an element, the following rules
   * are applied to obtain best guesses for the expected values for that
   * element's tests:
   *
   *   - The initial expected values arrays are:
   *
   *       [['', '', '']]
   *
   *   - If the element being tested is a number, the expected operand value
   *     of the last expected value array is set and changed as described in the
   *     comments for the |testNumber_()| method above.
   *
   *   - If the element being tested is the '+ / -' operation the expected
   *     values arrays will be changed as follows:
   *
   *       [*, [x, y, '']]     -> [*, [x, y, '']]
   *       [*, [x, y, z]]      -> [*, [x, y, -z]
   *       [*, [x, y, z1, z2]] -> [*, [x, y, -z1z2]
   *
   *   - If the element |e| being tested is the '+', '-', '*', or '/' operation
   *     the expected values will be changed as follows:
   *
   *       [*, [x, y, '']]     -> [*, ['', e, '']]
   *       [*, [x, y, z]]      -> [*, [z, y, z], ['', e, '']]
   *       [*, [x, y, z1, z2]] -> [*, [z1z2, y, z1z2], ['', e, '']]
   *
   *   - If the element being tested is the '=' command, the expected values
   *     will be changed as follows:
   *
   *       [*, ['', '', '']]   -> [*, [], ['0', '', '0']]
   *       [*, [x, y, '']]     -> [*, [x, y, z], [], ['0', '', '0']]
   *       [*, [x, y, z]]      -> [*, [x, y, z], [], [z, '', z]]
   *       [*, [x, y, z1, z2]] -> [*, [x, y, z], [], [z1z2, '', z1z2]]
   *
   * So for example this call:
   *
   *   run.testSequence_('My Test', '12 + 34 - 56 = [][-10]')
   *
   * would yield the following tests:
   *
   *   run.testInput_(controller, '1', [['', '', '1']]);
   *   run.testInput_(controller, '2', [['', '', '12']]);
   *   run.testInput_(controller, '+', [['12', '', '12'], ['', '+', '']]);
   *   run.testInput_(controller, '3', [['12', '', '12'], ['', '+', '3']]);
   *   run.testInput_(controller, '4', [..., ['', '+', '34']]);
   *   run.testInput_(controller, '-', [..., ['34', '', '34'], ['', '-', '']]);
   *   run.testInput_(controller, '2', [..., ['34', '', '34'], ['', '-', '2']]);
   *   run.testInput_(controller, '1', [..., ..., ['', '-', '21']]);
   *   run.testInput_(controller, '=', [[], [-10, '', -10]]);
   */
  testSequence_: function(name, sequence) {
    var controller = new Controller(new Model(8), window.mockView.create());
    var expected = [['', '', '']];
    var elements = this.parseSequence_(sequence);
    for (var i = 0; i < elements.length; ++i) {
      if (!Array.isArray(elements[i])) {  // Skip over expected value arrays.
        // Update and ajust expectations.
        this.updatedExpectations_(expected, elements[i]);
        if (Array.isArray(elements[i + 1] && elements[i + 1][0]))
          expected = this.adjustExpectations_([], elements[i + 1], 0);
        else
          expected = this.adjustExpectations_(expected, elements, i + 1);
        // Test.
        if (elements[i].match(/^-?[\d.][\d.=]*$/))
          this.testNumber_(controller, elements[i], expected);
        else
          this.testInput_(controller, elements[i], expected);
      };
    }
  },

  /** @private */
  parseSequence_: function(sequence) {
    // Define the patterns used below.
    var ATOMS = /(-?[\d.][\d.=]*)|([+*/=~<CAE_-])/g;  // number || command
    var VALUES = /(\[[^\[\]]*\])/g;                   // expected values
    // Massage the sequence into a JSON array string, so '2 + 2 = [4]' becomes:
    sequence = sequence.replace(ATOMS, ',$1$2,');     // ',2, ,+, ,2, ,=, [,4,]'
    sequence = sequence.replace(/\s+/g, '');          // ',2,,+,,2,,=,[,4,]'
    sequence = sequence.replace(VALUES, ',$1,');      // ',2,,+,,2,,=,,[,4,],'
    sequence = sequence.replace(/,,+/g, ',');         // ',2,+,2,=,[,4,],'
    sequence = sequence.replace(/\[,/g, '[');         // ',2,+,2,=,[4,],'
    sequence = sequence.replace(/,\]/g, ']');         // ',2,+,2,=,[4],'
    sequence = sequence.replace(/(^,)|(,$)/g, '');    // '2,+,2,=,[4]'
    sequence = sequence.replace(ATOMS, '"$1$2"');     // '"2","+","2","=",["4"]'
    sequence = sequence.replace(/"_"/g, '""');        // '"2","+","2","=",["4"]'
    // Fix some cases handled incorrectly by the massaging above, like the
    // original sequences '[_ _ 0=]' and '[-1]', which would have become
    // '["","","0","="]]' and '["-","1"]' respectively and would need to be
    // fixed to '["","","0="]]' and '["-1"]'respectively.
    sequence.replace(VALUES, function(match) {
      return match.replace(/(","=)|(=",")/g, '=').replace(/-","/g, '-');
    });
    // Return an array created from the resulting JSON string.
    return JSON.parse('[' + sequence + ']');
  },

  /** @private */
  updatedExpectations_: function(expected, element) {
    var last = expected[expected.length - 1];
    var empty = (last && !last[0] && !last[1] && !last[2] && !last[3]);
    var operand = last && last.slice(2).join('');
    var operation = element.match(/[+*/-]/);
    var equals = (element === '=');
    var negate = (element === '~');
    if (operation && !operand)
      expected.splice(-1, 1, ['', element, '']);
    else if (operation)
      expected.splice(-1, 1, [operand, last[1], operand], ['', element, '']);
    else if (equals && empty)
      expected.splice(-1, 1, [], [operand || '0', '', operand || '0']);
    else if (equals)
      expected.push([], [operand || '0', '', operand || '0']);
    else if (negate && operand)
      expected[expected.length - 1].splice(2, 2, '-' + operand);
  },

  /** @private */
  adjustExpectations_: function(expectations, adjustments, start) {
    var replace = !expectations.length;
    var adjustment, expectation;
    for (var i = 0; Array.isArray(adjustments[start + i]); ++i) {
      adjustment = adjustments[start + i];
      expectation = expectations[expectations.length - 1];
      if (adjustments[start + i].length != 1) {
        expectations.splice(-i - 1, replace ? 0 : 1);
        expectations.push(adjustments[start + i]);
      } else if (i || !expectation || !expectation.length || expectation[0]) {
        expectations.splice(-i - 1, replace ? 0 : 1);
        expectations.push([adjustment[0], '', adjustment[0]]);
      } else {
        expectations[expectations.length - i - 2][0] = adjustment[0];
      }
    }
    return expectations;
  },

  /**
   * @private
   * Tests how a calculator controller handles a sequence of digits and periods
   * representing a number. During the test, the expected operand values are
   * updated before each digit and period of the input according to these rules:
   *
   *   - If the last of the passed in expected values arrays has an expected
   *     accumulator value, add an empty expected values array and proceed
   *     according to the rules below with this new array.
   *
   *   - If the last of the passed in expected values arrays has no expected
   *     operand value and no expected operand prefix and suffix values, the
   *     expected operand used for the tests will start with the first digit or
   *     period of the numeric sequence and the following digits and periods of
   *     that sequence will be appended to that expected operand before each of
   *     the following digits and periods in the test.
   *
   *   - If the last of the passed in expected values arrays has a single
   *     expected operand value instead of operand prefix and suffix values, the
   *     expected operand used for the tests will start with the first character
   *     of that operand value and one additional character of that value will
   *     be added to the expected operand before each of the following digits
   *     and periods in the tests.
   *
   *   - If the last of the passed in expected values arrays has operand prefix
   *     and suffix values instead of an operand value, the expected operand
   *     used for the tests will start with the prefix value and the first
   *     character of the suffix value, and one character of that suffix value
   *     will be added to the expected operand before each of the following
   *     digits and periods in the tests.
   *
   *   - In all of these cases, leading zeros and occurrences of the '='
   *     character in the expected operand value will be ignored.
   *
   * For example the sequence of calls:
   *
   *   run.testNumber_(controller, '00', [[x, y, '0=']])
   *   run.testNumber_(controller, '1.2.3', [[x, y, '1.2=3']])
   *   run.testNumber_(controller, '45', [[x, y, '1.23', '45']])
   *
   * would yield the following tests:
   *
   *   run.testInput_(controller, '0', [[x, y, '0']]);
   *   run.testInput_(controller, '0', [[x, y, '0']]);
   *   run.testInput_(controller, '1', [[x, y, '1']]);
   *   run.testInput_(controller, '.', [[x, y, '1.']]);
   *   run.testInput_(controller, '2', [[x, y, '1.2']]);
   *   run.testInput_(controller, '.', [[x, y, '1.2']]);
   *   run.testInput_(controller, '3', [[x, y, '1.23']]);
   *   run.testInput_(controller, '4', [[x, y, '1.234']]);
   *   run.testInput_(controller, '5', [[x, y, '1.2345']]);
   *
   * It would also changes the expected value arrays to the following:
   *
   *   [[x, y, '1.2345']]
   */
  testNumber_: function(controller, number, expected) {
    var last = expected[expected.length - 1];
    var prefix = (last && !last[0] && last.length > 3 && last[2]) || '';
    var suffix = (last && !last[0] && last[last.length - 1]) || number;
    var append = (last && !last[0]) ? ['', last[1], ''] : ['', '', ''];
    var start = (last && !last[0]) ? -1 : expected.length;
    var count = (last && !last[0]) ? 1 : 0;
    expected.splice(start, count, append);
    for (var i = 0; i < number.length; ++i) {
      append[2] = prefix + suffix.slice(0, i + 1);
      append[2] = append[2].replace(/^0+([0-9])/, '$1').replace(/=/g, '');
      this.testInput_(controller, number[i], expected);
    }
  },

  /**
   * @private
   * Tests how a calculator controller handles a single element of input,
   * logging the state of the controller before and after the test.
   */
  testInput_: function(controller, input, expected) {
    var prefix = ['"', this.NAMES[input] || input, '": '];
    var before = this.addDescription_(prefix, controller, ' => ');
    var display = controller.view.testButton(this.BUTTONS[input]);
    var actual = display.slice(-expected.length);
    this.verify(expected, actual, this.getDescription_(before, controller));
  },

  /** @private */
  areEqual_: function(x, y) {
    return Array.isArray(x) ? this.areArraysEqual_(x, y) : (x == y);
  },

  /** @private */
  areArraysEqual_: function(a, b) {
    return Array.isArray(a) &&
           Array.isArray(b) &&
           a.length === b.length &&
           a.every(function(element, i) {
             return this.areEqual_(a[i], b[i]);
           }, this);
  },

  /** @private */
  getDescription_: function(prefix, object, suffix) {
    var strings = Array.isArray(prefix) ? prefix : prefix ? [prefix] : [];
    return this.addDescription_(strings, object, suffix).join('');
  },

  /** @private */
  addDescription_: function(prefix, object, suffix) {
    var strings = Array.isArray(prefix) ? prefix : prefix ? [prefix] : [];
    if (Array.isArray(object)) {
      strings.push('[', '');
      object.forEach(function(element) {
        this.addDescription_(strings, element, ', ');
      }, this);
      strings.pop();  // Pops the last ', ', or pops '' for empty arrays.
      strings.push(']');
    } else if (typeof object === 'number') {
      strings.push('#');
      strings.push(String(object));
    } else if (typeof object === 'string') {
      strings.push('"');
      strings.push(object);
      strings.push('"');
    } else if (object instanceof Controller) {
      strings.push('(');
      this.addDescription_(strings, object.model.accumulator, ' ');
      this.addDescription_(strings, object.model.operator, ' ');
      this.addDescription_(strings, object.model.operand, ' | ');
      this.addDescription_(strings, object.model.defaults.operator, ' ');
      this.addDescription_(strings, object.model.defaults.operand, ')');
    } else {
      strings.push(String(object));
    }
    strings.push(suffix || '');
    return strings;
  },

  /** @private */
  formatNumber_: function(number, digits) {
    var string = String(number);
    var array = Array(Math.max(digits - string.length, 0) + 1);
    array[array.length - 1] = string;
    return array.join('0');
  }

};
