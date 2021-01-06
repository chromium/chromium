// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function View(window) {
  this.display = window.document.querySelector('#calculator-display');
  this.buttons = window.document.querySelectorAll('#calculator-buttons button');
  window.addEventListener('keydown', this.handleKey_.bind(this));
  Array.prototype.forEach.call(this.buttons, function(button) {
    button.addEventListener('click', this.handleClick_.bind(this));
    button.addEventListener('mousedown', this.handleMouse_.bind(this));
    button.addEventListener('touchstart', this.handleTouch_.bind(this));
    button.addEventListener('touchmove', this.handleTouch_.bind(this));
    button.addEventListener('touchend', this.handleTouchEnd_.bind(this));
    button.addEventListener('touchcancel', this.handleTouchEnd_.bind(this));
  }, this);
}

View.prototype.clearDisplay = function(values) {
  this.display.innerHTML = '';
  this.addValues(values);
};

View.prototype.addResults = function(values) {
  this.appendChild_(this.display, null, 'div', 'hr');
  this.addValues(values);
};

View.prototype.addValues = function(values) {
  var equation = this.makeElement_('div', 'equation');
  this.appendChild_(equation, null, 'span', 'accumulator', values.accumulator);
  this.appendChild_(equation, null, 'span', 'operation');
  this.appendChild_(equation, '.operation', 'span', 'operator');
  this.appendChild_(equation, '.operation', 'span', 'operand', values.operand);
  this.appendChild_(equation, '.operator', 'div', 'spacer');
  this.appendChild_(equation, '.operator', 'div', 'value', values.operator);
  this.setAttribute_(equation, '.accumulator', 'aria-hidden', 'true');
  this.display.appendChild(equation).scrollIntoView();
};

View.prototype.setValues = function(values) {
  var equation = this.display.lastElementChild;
  this.setContent_(equation, '.accumulator', values.accumulator || '');
  this.setContent_(equation, '.operator .value', values.operator || '');
  this.setContent_(equation, '.operand', values.operand || '');
};

View.prototype.getValues = function() {
  var equation = this.display.lastElementChild;
  return {
    accumulator: this.getContent_(equation, '.accumulator') || null,
    operator: this.getContent_(equation, '.operator .value') || null,
    operand: this.getContent_(equation, '.operand') || null,
  };
};

/** @private */
View.prototype.handleKey_ = function(event) {
  this.onKey.call(this, event.shiftKey ? ('^' + event.which) : event.which);
}

/** @private */
View.prototype.handleClick_ = function(event) {
  this.onButton.call(this, event.target.dataset.button)
}

/** @private */
View.prototype.handleMouse_ = function(event) {
  event.target.setAttribute('data-active', 'mouse');
}

/** @private */
View.prototype.handleTouch_ = function(event) {
  event.preventDefault();
  this.handleTouchChange_(event.touches[0]);
}

/** @private */
View.prototype.handleTouchEnd_ = function(event) {
  this.handleTouchChange_(null);
}

/** @private */
View.prototype.handleTouchChange_ = function(location) {
  var previous = this.touched;
  if (!this.isInButton_(previous, location)) {
    this.touched = this.findButtonContaining_(location);
    if (previous)
      previous.removeAttribute('data-active');
    if (this.touched) {
      this.touched.setAttribute('data-active', 'touch');
      this.onButton.call(this, this.touched.dataset.button);
    }
  }
}

/** @private */
View.prototype.findButtonContaining_ = function(location) {
  var found;
  for (var i = 0; location && i < this.buttons.length && !found; ++i) {
    if (this.isInButton_(this.buttons[i], location))
      found = this.buttons[i];
  }
  return found;
}

/** @private */
View.prototype.isInButton_ = function(button, location) {
  var bounds = location && button && button.getClientRects()[0];
  var x = bounds && location.clientX;
  var y = bounds && location.clientY;
  var x1 = bounds && bounds.left;
  var x2 = bounds && bounds.right;
  var y1 = bounds && bounds.top;
  var y2 = bounds && bounds.bottom;
  return (bounds && x >= x1 && x < x2 && y >= y1 && y < y2);
}

/** @private */
View.prototype.makeElement_ = function(tag, classes, content) {
  var element = this.display.ownerDocument.createElement(tag);
  element.setAttribute('class', classes);
  element.textContent = content || '';
  return element;
};

/** @private */
View.prototype.appendChild_ = function(root, selector, tag, classes, content) {
  var parent = (root && selector) ? root.querySelector(selector) : root;
  parent.appendChild(this.makeElement_(tag, classes, content));
};

/** @private */
View.prototype.setAttribute_ = function(root, selector, name, value) {
  var element = root && root.querySelector(selector);
  if (element)
    element.setAttribute(name, value);
};

/** @private */
View.prototype.setContent_ = function(root, selector, content) {
  var element = root && root.querySelector(selector);
  if (element)
    element.textContent = content || '';
};

/** @private */
View.prototype.getContent_ = function(root, selector) {
  var element = root && root.querySelector(selector);
  return element ? element.textContent : null;
};
