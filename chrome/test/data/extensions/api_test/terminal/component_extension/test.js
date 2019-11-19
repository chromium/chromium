// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var shellCommand = 'shell\n';
var catCommand = 'cat\n';
var catErrCommand = 'cat 1>&2\n';

// Ensure this has all distinct characters.
var testLine = 'abcdefgh\n';

var startCharacter = '#';

var croshName = 'crosh';
var invalidName = 'some name';

var invalidNameError = 'Invalid process name: some name';

var testLineNum = 10;
var testProcessTotal = 2;

var testProcessCount = 0;
var testProcesses = [];

function TestProcess(id, type) {
  this.id_ = id;
  this.type_= type;

  this.lineExpectation_ = '';
  this.linesLeftToCheck_ = -1;
  // We receive two streams from the process.
  this.checkedStreamEnd_ = [0, 0];

  this.closed_ = false;
  this.startCharactersFound_ = 0;
  this.started_ = false;
};

// Method to test validity of received input. We will receive two streams of
// the same data. (input will be echoed twice by the testing process). Each
// stream will contain the same string repeated |kTestLineNum| times. So we
// have to match 2 * |kTestLineNum| lines. The problem is the received lines
// from different streams may be interleaved (e.g. we may receive
// abc|abcdef|defgh|gh). To deal with that, we allow to test received text
// against two lines. The lines MUST NOT have two same characters for this
// algorithm to work.
TestProcess.prototype.testExpectation = function(text) {
  chrome.test.assertTrue(this.linesLeftToCheck_ >= 0,
                         "Test expectations not set.")
  for (var i = 0; i < text.length; i++) {
    if (this.processReceivedCharacter_(text[i], 0))
      continue;
    if (this.processReceivedCharacter_(text[i], 1))
      continue;
    chrome.test.fail("Received: " + text);
  }
};

TestProcess.prototype.processReceivedCharacter_ = function(char, stream) {
  if (this.checkedStreamEnd_[stream] >= this.lineExpectation_.length)
    return false;

  var expectedChar = this.lineExpectation_[this.checkedStreamEnd_[stream]];
  if (expectedChar != char)
    return false

  this.checkedStreamEnd_[stream]++;

  if (this.checkedStreamEnd_[stream] == this.lineExpectation_.length &&
      this.linesLeftToCheck_ > 0) {
    this.checkedStreamEnd_[stream] = 0;
    this.linesLeftToCheck_--;
  }
  return true;
}

TestProcess.prototype.testOutputType = function(receivedType) {
  if (receivedType == 'exit')
    chrome.test.assertTrue(this.done());
  else
    chrome.test.assertEq('stdout', receivedType);
};

TestProcess.prototype.id = function() {
  return this.id_;
};

TestProcess.prototype.started = function() {
  return this.started_;
};

TestProcess.prototype.done = function() {
  return this.checkedStreamEnd_[0] == this.lineExpectation_.length &&
         this.checkedStreamEnd_[1] == this.lineExpectation_.length &&
         this.linesLeftToCheck_ == 0;
};

TestProcess.prototype.isClosed = function() {
  return this.closed_;
};

TestProcess.prototype.setClosed = function() {
  this.closed_ = true;
};

TestProcess.prototype.canStart = function() {
  return (this.startCharactersFound_ == 2);
};

TestProcess.prototype.startCharacterFound = function() {
  this.startCharactersFound_++;
};

TestProcess.prototype.getCatCommand = function() {
  if (this.type_ == "stdout")
    return catCommand;
  return catErrCommand;
};

TestProcess.prototype.addLineExpectation = function(line, times) {
  this.lineExpectation_ = line.replace(/\n/g, "\r\n");
  this.linesLeftToCheck_ = times - 2;
};

// Set of commands we use to setup terminal for testing (start cat) will produce
// some output. We don't care about that output, to avoid having to set that
// output in test expectations, we will send |startCharacter| right after cat is
// started. After we detect second |startCharacter|s in output, we know process
// won't produce any output by itself, so it is safe to start actual test.
TestProcess.prototype.maybeKickOffTest = function(text) {
  var index = 0;
  while (index != -1) {
    index = text.indexOf(startCharacter, index);
    if (index != -1) {
      this.startCharacterFound();
      if (this.canStart()) {
        this.kickOffTest_(testLine, testLineNum);
        return;
      }
      index++;
    }
  }
};

TestProcess.prototype.kickOffTest_ = function(line, lineNum) {
  this.started_ = true;
  // Each line will be echoed twice.
  this.addLineExpectation(line, lineNum * 2);

  for (var i = 0; i < lineNum; i++)
    chrome.terminalPrivate.sendInput(this.id_, line,
        function (result) {
          chrome.test.assertTrue(result);
        }
  );
};


function getProcessIndexForId(id) {
  for (var i = 0; i < testProcessTotal; i++) {
    if (testProcesses[i] && id == testProcesses[i].id())
      return i;
  }
  return undefined;
};

function processOutputListener(id, type, text) {
  var processIndex = getProcessIndexForId(id);
  if (processIndex == undefined)
    return;

  var process = testProcesses[processIndex];

  if (!process.started()) {
    process.maybeKickOffTest(text);
    return;
  }

  process.testOutputType(type);

  process.testExpectation(text);

  if (process.done())
    closeTerminal(processIndex);
};

function maybeEndTest() {
  for (var i = 0; i < testProcessTotal; i++) {
    if (!testProcesses[i] || !testProcesses[i].isClosed())
      return;
  }

  chrome.test.succeed();
};

function closeTerminal(index) {
  var process = testProcesses[index];
  chrome.terminalPrivate.closeTerminalProcess(
      process.id(),
      function(result) {
        chrome.test.assertTrue(result);
        process.setClosed();
        maybeEndTest();
      }
  );
};

function initTest(process) {
  var sendStartCharacter = function() {
      chrome.terminalPrivate.sendInput(
          process.id(),
          startCharacter + '\n',
          function(result) {
              chrome.test.assertTrue(result);
          }
      );
  };

  var startCat = function() {
      chrome.terminalPrivate.sendInput(
          process.id(),
          process.getCatCommand(),
          function(result) {
            chrome.test.assertTrue(result);
            sendStartCharacter();
          }
      );
  };

  chrome.terminalPrivate.sendInput(
      process.id(),
      shellCommand,
      function(result) {
        chrome.test.assertTrue(result);
        startCat();
      }
  );
};

chrome.test.runTests([
  function terminalTest() {
    chrome.terminalPrivate.onProcessOutput.addListener(processOutputListener);

    for (var i = 0; i < testProcessTotal; i++) {
      chrome.terminalPrivate.openTerminalProcess(croshName, function(result) {
          chrome.test.assertTrue(typeof result == 'string');
          // The handled returned is basically a guid, but we don't want to
          // enforce that API, so just enforce the string contains at least a
          // certain number of bytes for general randomness/uniqueness.
          chrome.test.assertTrue(result.length > 18);
          var type = (testProcessCount % 2) ? 'stderr' : 'stdout';
          var newProcess = new TestProcess(result, type);
          testProcesses[testProcessCount] = newProcess;
          testProcessCount++;
          initTest(newProcess);
      });
    }
  },

  function invalidProcessNameTest() {
    chrome.terminalPrivate.openTerminalProcess(invalidName,
        chrome.test.callbackFail(invalidNameError));
  },

  function settingsTest() {
    chrome.terminalPrivate.onSettingsChanged.addListener((settings) => {
      // 3. Event is fired - {'k': 'v'}.
      chrome.test.assertEq(1, Object.keys(settings).length);
      chrome.test.assertEq('v', settings['k']);

      // 4. Get settings - {'k': 'v'}.
      chrome.terminalPrivate.getSettings(
          chrome.test.callbackPass((settings) => {
            chrome.test.assertEq(1, Object.keys(settings).length);
            chrome.test.assertEq('v', settings['k']);
            chrome.test.succeed();
          }));
    });

    // 1. Get settings - {}.
    chrome.terminalPrivate.getSettings(chrome.test.callbackPass((settings) => {
      chrome.test.assertEq(0, Object.keys(settings).length);

      // 2. Set {'k': 'v'}.
      chrome.terminalPrivate.setSettings({k: 'v'}, chrome.test.callbackPass());
    }));
  }
]);
