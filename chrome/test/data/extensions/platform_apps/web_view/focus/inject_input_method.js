// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

function Tester() {
  this.queue_ = [];
  this.inputElement1_ = null;
  this.inputElement2_ = null;
  this.embedderChannel_ = null;

  // Various states for the first <input>.
  this.inputState_ = {
    // Value change related.
    // Whether we're waiting to see an 'oninput' event.
    waitingForInput: false,
    waitingForInputCallback: null,
    seenInput: false,
    lastInputValue: null,
    // Focus related.
    // Whether we're waiting to see a 'focus' event.
    waitingForFocus: false,
    seenFocus: false
  };
  window.addEventListener('message', this.onMessage.bind(this));
};

Tester.prototype.queueMessage_ = function(data) {
  this.queue_.push(data);
};

// Sends message to the embedder via postmessage.
Tester.prototype.sendMessage = function(data) {
  LOG('Tester.prototype.sendMessage: ' + data);
  if (!this.embedderChannel_) {
    this.queueMessage_(data);
    return;
  }

  this.embedderChannel_.postMessage(JSON.stringify(data), '*');
  while (this.queue_.length) {
    var qdata = this.queue_.pop();
    this.embedderChannel_.postMessage(JSON.stringify(qdata), '*');
  }
};

Tester.prototype.sendWaitForInputResponse_ = function(setSelction) {
  LOG('Tester.sendWaitForInputResponse_, setSelction: ' + setSelction);
  if (setSelction) {
    this.sendMessage(
        ['response-waitForOnInputAndSelect', this.inputState_.lastInputValue]);
    this.inputElement1_.setSelectionRange(6, 6);
  } else {
    this.sendMessage(
        ['response-waitForOnInput', this.inputState_.lastInputValue]);
  }
};

Tester.prototype.setUp = function() {
  if (!this.inputElement1_) {
    this.inputElement1_ = document.createElement('input');
    this.inputElement2_ = document.createElement('input');
    document.body.appendChild(this.inputElement1_);
    document.body.appendChild(this.inputElement2_);

    this.inputElement1_.onfocus = this.onElement1Focus_.bind(this);
    this.inputElement1_.oninput = this.onElement1Input_.bind(this);
  }

  // The message might be queued and sent once |embedderChannel_|
  // connection is established.
  this.sendMessage(['response-inputMethodPreparedForFocus']);
};

// Receives command from the embedder via postMessage.
Tester.prototype.onMessage = function(e) {
  LOG('Tester.onMessage: e.data = ' + e.data);
  if (!this.embedderChannel_) {
    this.embedderChannel_ = e.source;
  }

  var data = JSON.parse(e.data);
  switch (data[0]) {
    case 'connect':
      this.embedderChannel_ = e.source;
      this.sendMessage(['connected']);
      break;
    case 'request-waitForFocus':
      this.waitForFocus_();
      break;
    case 'request-waitForOnInput':
      this.waitForOnInput_(this.sendWaitForInputResponse_.bind(this, false));
      break;
    case 'request-waitForOnInputAndSelect':
      this.waitForOnInput_(this.sendWaitForInputResponse_.bind(this, true));
      break;
    case 'request-valueAfterExtendSelection':
      this.sendMessage(['response-valueAfterExtendSelection',
                        this.inputElement1_.value]);
      break;
    default:
      LOG('Curious message from embedder: ' + data);
      // Sending a bad message will result in test failure.
      this.sendMessage(['bogus']);
      break;
  }
};

Tester.prototype.onElement1Focus_ = function() {
  if (this.inputState_.waitingForFocus) {
    this.inputState_.waitingForFocus = false;
    this.sendMessage(['response-seenFocus']);
    return;
  }
  // Record the fact that we've seen a focus.
  this.inputState_.seenFocus = true;
};

Tester.prototype.onElement1Input_ = function() {
  LOG('Tester.onElement1Input_');
  this.inputState_.seenInput = true;
  this.inputState_.lastInputValue = this.inputElement1_.value;
  LOG('this.lastInputValue: ' + this.inputState_.lastInputValue);
  this.didInputStateChange_();
};

// Waits for oninput event to fire on the first <input>.
// Upon receiving the event |doneCallback| is invoked.
Tester.prototype.waitForOnInput_ = function(doneCallback) {
  this.inputState_.waitingForInput = true;
  this.inputState_.waitingForInputCallback = doneCallback;
  this.didInputStateChange_();
};

Tester.prototype.didInputStateChange_ = function() {
  if (this.inputState_.waitingForInput && this.inputState_.seenInput) {
    this.inputState_.waitingForInput = false;
    this.inputState_.seenInput = false;

    if (this.inputState_.waitingForInputCallback) {
      this.inputState_.waitingForInputCallback();
      this.inputState_.waitingForInputCallback = null;
    }
    this.inputState_.lastInputValue = '';
  }
};

// Waits for the first <input> to be focused.
Tester.prototype.waitForFocus_ = function() {
  if (this.inputState_.seenFocus) {
    this.sendMessage(['response-seenFocus']);
    this.inputState_.seenFocus = false;
    return;
  }

  this.inputState_.waitingForFocus = true;
  this.inputElement1_.focus();
};

var tester = new Tester();
tester.setUp();
