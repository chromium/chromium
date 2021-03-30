// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Model(precision) {
  this.reset_({precision: precision});
}

/**
 * Handles a calculator key input, updating the calculator state accordingly and
 * returning an object with 'accumulator', 'operator', and 'operand' properties
 * representing that state.
 *
 * @private
 */
Model.prototype.handle = function(input) {
  switch (input) {
    case '+':
    case '-':
    case '/':
    case '*':
      // For operations, ignore the last operator if no operand was entered,
      // otherwise perform the current calculation before setting the new
      // operator. In either case, clear the operand and the defaults.
      var operator = this.operand && this.operator;
      var result = this.calculate_(operator, this.operand);
      return this.reset_({accumulator: result, operator: input});
    case '=':
      // For the equal sign, perform the current calculation and save the
      // operator and operands used as defaults, or if there is no current
      // operator, use the default operators and operands instead. In any case,
      // clear the operator and operand and return a transient state with a '='
      // operator.
      var operator = this.operator || this.defaults.operator;
      var operand = this.operator ? this.operand : this.defaults.operand;
      var result = this.calculate_(operator, operand);
      var defaults = {operator: operator, operand: this.operand};
      return this.reset_({accumulator: result, defaults: defaults});
    case 'AC':
      return this.reset_({});
    case 'C':
      return this.operand  ? this.set_({operand: null}) :
             this.operator ? this.set_({operator: null}) :
                             this.handle('AC');
    case 'back':
      var length = (this.operand || '').length;
      return (length > 1)  ? this.set_({operand: this.operand.slice(0, -1)}) :
             this.operand  ? this.set_({operand: null}) :
                             this.set_({operator: null});
    case '+ / -':
      var initial = (this.operand || '0')[0];
      return (initial === '-') ? this.set_({operand: this.operand.slice(1)}) :
             (initial !== '0') ? this.set_({operand: '-' + this.operand}) :
                                 this.set_({});
    default:
      var operand = (this.operand || '0') + input;
      var duplicate = (operand.replace(/[^.]/g, '').length > 1);
      var overflow = (operand.replace(/[^0-9]/g, '').length > this.precision);
      return operand.match(/^0[0-9]/)  ? this.set_({operand: operand[1]}) :
             (!duplicate && !overflow) ? this.set_({operand: operand}) :
                                         this.set_({});
  }
}

/**
 * Reset the model's state to the passed in state.
 *
 * @private
 */
Model.prototype.reset_ = function(state) {
  this.accumulator = this.operand = this.operator = null;
  this.defaults = {operator: null, operand: null};
  return this.set_(state);
}

/**
 * Selectively replace the model's state with the passed in state.
 *
 * @private
 */
Model.prototype.set_ = function(state) {
  var ifDefined = function(x, y) { return (x !== undefined) ? x : y; };
  var precision = (state && state.precision) || this.precision || 9;
  this.precision = Math.min(Math.max(precision, 1), 9);
  this.accumulator = ifDefined(state && state.accumulator, this.accumulator);
  this.operator = ifDefined(state && state.operator, this.operator);
  this.operand = ifDefined(state && state.operand, this.operand);
  this.defaults = ifDefined(state && state.defaults, this.defaults);
  return this;
}

/**
 * Performs a calculation based on the passed in operator and operand, updating
 * the model's state with the operator and operand used but returning the result
 * of the calculation instead of updating the model's state with it.
 *
 * @private
 */
Model.prototype.calculate_ = function(operator, operand) {
  var x = Number(this.accumulator) || 0;
  var y = operand ? Number(operand) : x;
  this.set_({accumulator: String(x), operator: operator, operand: String(y)});
  return (this.operator == '+') ? this.round_(x + y) :
         (this.operator == '-') ? this.round_(x - y) :
         (this.operator == '*') ? this.round_(x * y) :
         (this.operator == '/') ? this.round_(x / y) :
                                  this.round_(y);
}

/**
 * Returns the string representation of the passed in value rounded to the
 * model's precision, or "E" on overflow.
 *
 * @private
 */
Model.prototype.round_ = function(x) {
  var exponent = Number(x.toExponential(this.precision - 1).split('e')[1]);
  var digits = this.digits_(exponent);
  var exponential = x.toExponential(digits).replace(/\.?0+e/, 'e');
  var fixed = (Math.abs(exponent) < this.precision && exponent > -7);
  return !digits ? 'E' : fixed ? String(Number(exponential)) : exponential;
}

/**
 * Returns the appropriate number of digits to include of a number based on
 * its size.
 *
 * @private
 */
Model.prototype.digits_ = function(exponent) {
  return (isNaN(exponent) || exponent < -199 || exponent > 199) ? 0 :
         (exponent < -99) ? (this.precision - 1 - 5) :
         (exponent < -9) ? (this.precision - 1 - 4) :
         (exponent < -6) ? (this.precision - 1 - 3) :
         (exponent < 0) ? (this.precision - 1 + exponent) :
         (exponent < this.precision) ? (this.precision - 1) :
         (exponent < 10) ? (this.precision - 1 - 3) :
         (exponent < 100) ? (this.precision - 1 - 4) :
                            (this.precision - 1 - 5);
}
