// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.runTests = function(log) {
  var run = window.calculatorTestRun.create();
/*
  // ---------------------------------------------------------------------------
  // Test fixes for <http://crbug.com/156448>:

  run.test('Twenty eight can be divided by three', '28 / 3 = [9.3333333]');
  run.test('Twenty nine can be divided by three', '29 / 3 = [9.6666667]');
  run.test('Thirty can be divided by three', '30 / 3 = [10]');
  run.test('Thirty one can be divided by three', '31 / 3 = [10.333333]');
  run.test('Thirty two can be divided by three', '32 / 3 = [10.666667]');
  run.test('Thirty three can be divided by three', '33 / 3 = [11]');

  // ---------------------------------------------------------------------------
  // Test fixes for <http://crbug.com/156449>:

  // run.test('Equals without operator results in operand value',
  //          '123 = [123]');

  // run.test('Operations without operands uses default operands',
  //          '2 + = [[2 _ 2][_ + 2][][4 _ 4]]');

  // TODO(dharcourt): Test the display for the expected output.
  // run.test('Successive operators replace each other.',
  //          '123 + - * / [123 / _] and no previous [* + *]');

  // ---------------------------------------------------------------------------
  // Test fixes for <http://crbug.com/156450>:

  // run.test('Operand can be erased and replaced',
  //          '123 + 456 < < < < 789 = [789]');

  // TODO(dharcourt): Test the display for the expected output.
  // run.test('Operators can be erased and replaced.',
  //          '123 + 456 < < < < < * 2 = [246]');

  // TODO(dharcourt): Test the display for the expected output.
  // run.test('Erase is ignored after equals.', '123 + 456 = < [579]');
  // run.test('Erase is ignored after zero result.', '123 - 123 = < [9]');

  // TODO(dharcourt): Test the display for the expected output.
  // run.test('Erasing an operand makes it blank.',
  //          '123 + 456 < < < [_ + _]');

  // ---------------------------------------------------------------------------
  // Test fixes for <http://crbug.com/156451>:

  // run.test('Negation applies to zero', '~ [[-0]]');
  // run.test('Negation applies before input', '~ 123 = [[-123]]');
  // run.test('Negation applies after input is erased',
  //          '123 < < < ~ 456 [[-456]]');
  // run.test('Negation is preserved when input is erased',
  //          '123 ~ < < < 456 [[-456]]');
  // run.test('Negation supports small values',
  //          '0.0000001 ~ [[-0.0000001]]');

  // run.test('Addition resets negated zeros', '~ + [_ + [0]]');
  // run.test('Subtraction resets negated zeros', '~ - [_ - [0]]');
  // run.test('Multiplication resets negated zeros', '~ * [_ * [0]]');
  // run.test('Division resets negated zeros', '~ / [_ / [0]]');
  // run.test('Equals resets negated zeros', '~ = [0 _ [0]]');

  // ---------------------------------------------------------------------------
  // Test fixes for <http://crbug.com/156452>:

  // TODO(dharcourt): Test the display for the expected output.
  // TODO(dharcourt): Make the test utilities support 'error'.
  // run.test('Errors results are spelled out', '1 / 0 = [[error]]');
  // run.test('Addition treats errors as zero',
  //          '1 / 0 = [error] + [0 + [0]] 123 = [123]');
  // run.test('Subtraction treats errors as zero',
  //          '1 / 0 = [error] - [0 - [0]] 123 = [-123]');
  // run.test('Multiplication treats errors as zero',
  //          '1 / 0 = [error] * [0 * [0]] 123 = [0]');
  // run.test('Division treats errors as zero',
  //          '1 / 0 = [error] / [0 / [0]] 123 = [0]');
  // run.test('Equals treats errors as zero',
  //         '1 / 0 = [error] = [0]');

  // ---------------------------------------------------------------------------
  // Test fixes for <http://crbug.com/156453>:

  // run.test('Common operations are reversible',
  //          '1 / 3 * 3 = [[1]]');

  // run.test('Large numbers are displayed as exponentials',
  //          '12345678 * 10 = [[1.23456e8]]');
  // run.test('Small numbers are displayed as exponentials',
  //          '0.0000001 / 10 = [[1e-8]]');

  // ---------------------------------------------------------------------------
  // Other tests.
  // TODO(dharcourt): Organize and beef up these tests.
  // TODO(dharcourt): Test {nega,posi}tive {under,over}flows.

  run.test("Initialization", function(controller) {
    run.verify(null, controller.model.accumulator, 'Accumulator');
    run.verify(null, controller.model.operator, 'Operator');
    run.verify(null, controller.model.operand, 'Operand');
    run.verify(null, controller.model.defaults.operator, 'Default Operator');
    run.verify(null, controller.model.defaults.operand, 'Defaults Operand');
  });

  run.test("AC", '1 + 2 = [3] 4 A [[_ _ 0]]');

  run.test("back", '1 + 2 < [_ + _] < [_ _ _] < [_ _ 0]');
  // TODO(dharcourt@chromium.org): The previous lines should be:
  //   '1 + 2 < [_ + _] < [_ _ 0] < [_ _ 0]'
  // TODO(dharcourt@chromium.org): Test more AC, C, back

  run.test("Miscellaneous Test A",
           '2 [_ _ 2] + [[2 _ 2][_ + _]] = [[2 _ 2][_ + _][][4 _ 4]]' +
           '          + [[2 _ 2][_ + _][][4 _ 4][_ + _]]' +
           '          = [[2 _ 2][_ + _][][4 _ 4][_ + _][][8 _ 8]]' +
           '          = [[2 _ 2][_ + _][][4 _ 4][_ + _][][8 _ 8][][12 _ 12]]');
  // TODO(dharcourt@chromium.org): The previous lines should be:
  //     '2 [_ _ 2] + [[2 _ 2][_ + _]] = [[2 _ 2][_ + 2][][4 _ 4]]' +
  //     '          + [[2 _ 2][_ + 2][][4 _ 4][_ + _]]' +
  //     '          = [[2 _ 2][_ + 2][][4 _ 4][_ + 4][][8 _ 8]]' +
  //     '          = [[2 _ 2][_ + 2][][4 _ 4][_ + 4][][8 _ 8][][12 _ 12]]');
*/
  run.test("Miscellaneous Test B", '2 * = [4] * [_ * _] = [16] = [64]');

  run.test("Miscellaneous Test C", '0.020202020 [_ _ 0.0202020==]');

  run.test("Miscellaneous Test D", '.2 [_ _ 0 .2]');

  run.test("Miscellaneous Test E", '0.00000014 [_ _ 0.0000001=]');

  run.test("Miscellaneous Test F", '0.10000004 [_ _ 0.1000000=]');

  run.test("Miscellaneous Test G", '0.12312312 [_ _ 0.1231231=]');

  run.test("Miscellaneous Test H", '1.231231234 [_ _ 1.2312312==]');

  run.test("Miscellaneous Test I", '123.1231234 [_ _ 123.12312==]');

  run.test("Miscellaneous Test J", '123123.1234 [_ _ 123123.12==]');

  run.test("Miscellaneous Test K", '12312312.34 [_ _ 12312312.==]');

  run.test("Miscellaneous Test L", '12312312.04 [_ _ 12312312.==]');

  run.test("Miscellaneous Test M", '1231231234 [_ _ 12312312==]');

  run.test("Miscellaneous Test N", '1 + 1 + [[1 _ 1][2 + 1][_ + _]] = [4]');

  run.test("Miscellaneous Test O", '1 + 1 [_ + 1] 2 [_ + 1 2]');

  run.test("Positive + Positive", '82959 + 4 = [82963]');

  run.test("Positive + Negative", '123 + 456~ = [-333]');

  run.test("Negative + Positive", '502~ + 385 = [-117]');

  run.test("Negative + Negative", '4296~ + 32~ = [-4328]');

  run.test("Positive + Zero", '23650 + 0 = [23650]');

  run.test("Negative + Zero", '489719~ + 0 = [-489719]');

  run.test("Zero + Positive", '0 + 4296 = [4296]');

  run.test("Zero + Negative", '0 + 322~ = [-322]');

  run.test("Zero + Zero", '0 + 0 = [0]');

  run.test("Addition Chain",
           '+ 5 + 3~ + [2] 2~ + [0] 1~ + [-1] 3 + [2] 0 = [2]');

  run.test("Positive - Positive", '4534 - 327 = [4207]');

  run.test("Subtraction Chain",
           '- 5 - [-5] 3~ - [-2] 2~ - [0] 1~ - [1] 3 - [-2] 0 = [-2]');

  run.test("Positive * Positive", '7459 * 660 = [4922940]');

  run.test("Multiplication Chain",
           '* 5 = [0] 1 * [1] 5 * [5] 2~ ' +
           '* [-10] 1.5 * [-15] 1~ * [15] 0 = [0]');

  run.test("Positive / Positive", '181 / 778 = [0.2326478]');

  run.test("Division Chain",
           '/ 5 = [0] 1 / [1] 5 / [0.2] 2~ ' +
           '/ [-0.1] 0.1 / [-1] 1~ / [1] 0 = [E]');

  run.test("Positive Decimal Plus Positive Decimal",
           '7.48 + 8.2 = [15.68]');

  run.test("Decimals with Leading Zeros",
           '0.0023 + 0.082 = [0.0843]');

  run.test("Addition and Subtraction Chain",
           '4 + [4] 1055226 - [1055230] 198067 = [857163]');

  run.test("Multiplication and Division Chain",
           '580 * [580] 544 / [315520] 64 = [4930]');

  run.test("Addition After Equals",
           '5138 + 3351301 = [3356439] 550  + 62338 = [62888]');

  run.test("Missing First Operand in Addition", '+ 9701389 = [9701389]');

  run.test("Missing First Operand in Subtraction", '- 1770 = [-1770]');

  run.test("Missing First Operand in Multiplication", '* 65146 = [0]');

  run.test("Missing First Operand in Division", '/ 8 = [0]');

  run.test("Missing Second Operand in Addition",
           '28637 + = [[28637 _ 28637][_ + _][][57274 _ 57274]]');
           // TODO(dharcourt): Previous line should be:
           //        '28637 + = [[28637 _ 28637][_ + 28637][][57274 _ 57274]');

  run.test("Missing Second Operand in Subtraction",
           '52288 - = [[52288 _ 52288][_ - _][][0 _ 0]]');
           // TODO(dharcourt): Previous line should be:
           //        '52288 - = [[52288 _ 52288][_ - 52288][][0 _ 0]]');

  run.test("Missing Second Operand in Multiplication",
           '17 * = [17 _ 17][_ * _][][289]');
           // TODO(dharcourt): Previous line should be:
           //        '17 * = [17 _ 17][_ * 17][][289]');

  run.test("Missing Second Operand in Division",
           '47 / = [47 _ 47][_ / _][][1]');
           // TODO(dharcourt): Previous line should be:
           //        '47 / = [47 _ 47][_ / 47][][1]');

  run.test("Missing Last Operand in Addition and Subtraction Chain",
           '8449 + 4205138 - [4213587] = [0]');

/* TODO(dharcourt): Fix & reactivate these:

  run.test("Leading zeros should be collapsed",
           '0 [_ _ [0]] 0 [_ _ [0]] 0 [_ _ [0]] ' +
           '2 [_ _ [2]] 0 [_ _ [2 0]] + [20 + _] ' +
           '0 [20 + [0]] 0 [20 + [0]] 0 [20 + [0]] ' +
           '2 [20 + [2]] 0 [20 + [2 0]] = [40 _ _]');

  run.test("Test utilities should correctly predict zeros collapse",
           '000 [[0==]] 20 [[20]] + 000 [[0==]] 20 [[20]] = [40]' +
           '00020 + 00020 = [40]');

*/

  if (log || typeof log === 'undefined')
    run.log();

  return run;
};
