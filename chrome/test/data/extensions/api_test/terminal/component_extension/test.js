// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var shellCommand = 'shell\n';
var catCommand = 'cat\n';
var catErrCommand = 'cat 1>&2\n';

// Ensure this has all distinct characters.
var testLine = 'abcdefgh\n';

var croshName = 'crosh';
var invalidName = 'some name';

var invalidNameError = 'Invalid process name: some name';

var testLineNum = 10;
var testProcessTotal = 2;

var testProcessCount = 0;
var testProcesses = [];

const decoder = new TextDecoder();

function TestProcess(id, type) {
  this.id_ = id;
  this.type_= type;

  // Start text to receive before we start matching lines.
  // We receive 2x each of:
  // type=stdout    | type=stderr
  // shell\r\n    7 | shell\r\n      7
  // cat\r\n      5 | cat 1>&2\r\n  10
  // ---------------------------------
  // 2 x SUM:    24 |               34
  this.startText_ = '';
  this.startTextLength_ = type === 'stdout' ? 24 : 34;

  this.lineExpectation_ = '';
  this.linesLeftToCheck_ = -1;
  // We receive two streams from the process.
  this.checkedStreamEnd_ = [0, 0];

  this.closed_ = false;
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
                         'Test expectations not set.')
  for (var i = 0; i < text.length; i++) {
    if (this.processReceivedCharacter_(text[i], 0))
      continue;
    if (this.processReceivedCharacter_(text[i], 1))
      continue;
    chrome.test.fail('Received: [' + text + ']');
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

TestProcess.prototype.getCatCommand = function() {
  if (this.type_ == 'stdout')
    return catCommand;
  return catErrCommand;
};

TestProcess.prototype.addLineExpectation = function(line, times) {
  this.lineExpectation_ = line.replace(/\n/g, '\r\n');
  this.linesLeftToCheck_ = times - 2;
};

// We first call 'shell' and 'cat' (stdout) / 'cat 1>&2' (stderr) to set up the
// terminal.  We start testing once we have received this text.
TestProcess.prototype.maybeKickOffTest = function(text) {
  this.startText_ += text;
  if (this.startText_.length > this.startTextLength_) {
     chrome.test.fail('Unexpected start text: [' + this.startText_ + ']');
  } else if (this.startText_.length === this.startTextLength_) {
    this.kickOffTest_(testLine, testLineNum);
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

function processOutputListener(id, type, data) {
  var processIndex = getProcessIndexForId(id);
  if (processIndex == undefined)
    return;

  const text = decoder.decode(data);

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
  var startCat = function() {
      chrome.terminalPrivate.sendInput(
          process.id(),
          process.getCatCommand(),
          function(result) {
            chrome.test.assertTrue(result);
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

  function prefsTest() {
    const pContainers = 'crostini.containers';
    const pSettings = 'crostini.terminal_settings';
    const pA11y = 'settings.accessibility';
    const paths = [pContainers, pSettings, pA11y, 'unknown-ignored'];
    const validateGetPrefs = (prefs, settingsLength) => {
      chrome.test.assertEq(3, Object.keys(prefs).length);
      chrome.test.assertTrue(Array.isArray(prefs[pContainers]));
      chrome.test.assertEq(0, prefs[pContainers].length);
      chrome.test.assertEq('object', typeof prefs[pSettings]);
      chrome.test.assertEq(
          settingsLength, Object.keys(prefs[pSettings]).length);
      chrome.test.assertEq('boolean', typeof prefs[pA11y]);
      chrome.test.assertFalse(prefs[pA11y]);
    };

    const listener = (prefs) => {
      // 3. Event is fired - only includes settings with {'k': 'v'}.
      chrome.test.assertEq(1, Object.keys(prefs).length);
      chrome.test.assertEq('object', typeof prefs[pSettings]);
      chrome.test.assertEq(1, Object.keys(prefs[pSettings]).length);
      chrome.test.assertEq('v', prefs[pSettings]['k']);

      // 4. Get prefs - settings has {'k': 'v'}, others unchanged.
      chrome.terminalPrivate.getPrefs(paths, (prefs) => {
        chrome.test.assertNoLastError();
        validateGetPrefs(prefs, 1);
        chrome.test.assertEq('v', prefs[pSettings]['k']);

        // 5. Cleanup.
        chrome.terminalPrivate.onPrefChanged.removeListener(listener);
        chrome.terminalPrivate.onPrefChanged.addListener(chrome.test.succeed);
        chrome.terminalPrivate.setPrefs(
            {[pSettings]: {}}, chrome.test.assertNoLastError);
      });
    };
    chrome.terminalPrivate.onPrefChanged.addListener(listener);

    // 1. Get prefs - 3 valid, plus another unknown (will be ignored).
    chrome.terminalPrivate.getPrefs(paths, (prefs) => {
        chrome.test.assertNoLastError();
        validateGetPrefs(prefs, 0);

        // 2. Set prefs - only settings allows write.
        chrome.terminalPrivate.setPrefs({
            [pContainers]: [{k1: 'v1'}, {k2: 'v2'}],
            [pSettings]: {k: 'v'},
            [pA11y]: true,
            'unknown-ignored': 'ignored',
          }, chrome.test.assertNoLastError);
    });
  },

  function invalidTerminalIdTest() {
    const foreign_id = (new URLSearchParams(location.search)).get('foreign_id');
    chrome.test.assertTrue(!!foreign_id);

    const callbackFail = chrome.test.callbackFail;

    [foreign_id, 'invalid id'].forEach((id) => {
      // Ideally, we will also want to test ackOutput, but it does not have a
      // result callback.
      chrome.terminalPrivate.closeTerminalProcess(
          id, callbackFail('invalid terminal id'));
      // If this manages to write to the `foreign_id` process, we should detect
      // some output in terminal_private_apitest.cc.
      chrome.terminalPrivate.sendInput(
          id, 'hello', callbackFail('invalid terminal id'));
      chrome.terminalPrivate.onTerminalResize(
          id, 10, 10, callbackFail('invalid terminal id'));
    });
  },
]);
