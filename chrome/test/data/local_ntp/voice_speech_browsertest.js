// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests the speech module of Voice Search on the local NTP.
 */

/**
 * Voice Search Speech module's object for test and setup functions.
 */
test.speech = {};

/**
 * ID of the fakebox microphone icon.
 * @const
 */
test.speech.FAKEBOX_MICROPHONE_ID = 'fakebox-microphone';

/**
 * A module configuration for the test.
 * @const
 */
test.speech.TEST_BASE_URL = 'https://google.com/';

/**
 * A module configuration for the test.
 * @const
 */
test.speech.TEST_STRINGS = {
  audioError: 'Audio error',
  details: 'Details',
  fakeboxMicrophoneTooltip: 'fakebox-microphone tooltip',
  languageError: 'Language error',
  learnMore: 'Learn more',
  listening: 'Listening',
  networkError: 'Network error',
  noTranslation: 'No translation',
  noVoice: 'No voice',
  otherError: 'Unknown error',
  permissionError: 'Permission error',
  ready: 'Ready',
  tryAgain: 'Try again',
  waiting: 'Waiting'
};

/**
 * Mock out the clock functions for testing timers.
 * @type {MockClock}
 */
test.speech.clock = new MockClock();

/**
 * Represents the URL of the opened tab.
 * @type {URL}
 */
test.speech.locationUrl = null;

/**
 * An empty object serving as a mock of |chrome.embeddedSearch.searchBox|.
 * @type {!object}
 */
test.speech.mockSearchBox = {};

/**
 * Keeps track of the number of |speech.recognition_| activations.
 * @type {number}
 */
test.speech.recognitionActiveCount = 0;

/**
 * Utility to mock out parts of the DOM.
 * @type {Replacer}
 */
test.speech.stubs = new Replacer();

/**
 * Utility to mock out the Speech Recognition API.
 * @type {Replacer}
 */
test.speech.recognitionStubs = new Replacer();

/**
 * Keeps track of the number of view activations.
 * @type {number}
 */
test.speech.viewActiveCount = 0;

/**
 * Mocks the current view state.
 * @type {object}
 */
test.speech.viewState = {};

/**
 * Represents the target of the view's window click event.
 * @type {object}
 */
test.speech.viewClickTarget = {};

/**
 * Set up the text DOM and test environment.
 */
test.speech.setUp = function() {
  setUpPage('voice-speech-template');

  // Uninitialize speech between tests to allow message listeners to reset.
  test.speech.unInitSpeech(
      $(test.speech.FAKEBOX_MICROPHONE_ID), test.speech.mockSearchBox);

  // Reset variable values.
  test.speech.clock.reset();
  test.speech.clock.install();
  test.speech.locationUrl = null;
  test.speech.recognitionActiveCount = 0;
  test.speech.stubs.reset();
  test.speech.recognitionStubs.reset();
  test.speech.viewActiveCount = 0;
  test.speech.viewState = {};

  // Mock speech functions.
  test.speech.stubs.replace(
      speech, 'navigateToUrl_', (url) => test.speech.locationUrl = url);

  // Mock view functions.
  test.speech.stubs.replace(view, 'hide', () => test.speech.viewActiveCount--);
  test.speech.stubs.replace(view, 'init', () => {});
  test.speech.stubs.replace(view, 'setTitles', () => {});
  test.speech.stubs.replace(view, 'onWindowClick_', (event) => {
    test.speech.viewClickTarget = event.target;
  });
  test.speech.stubs.replace(
      view, 'setReadyForSpeech', () => test.speech.viewState.ready = true);
  test.speech.stubs.replace(
      view, 'setReceivingSpeech', () => test.speech.viewState.receiving = true);
  test.speech.stubs.replace(view, 'show', () => test.speech.viewActiveCount++);
  test.speech.stubs.replace(
      view, 'showError', (error) => test.speech.viewState.error = error);
  test.speech.stubs.replace(view, 'updateSpeechResult', (interim, final) => {
    test.speech.viewState.interim = interim;
    test.speech.viewState.final = final;
  });

  // Mock window functions.
  test.speech.stubs.replace(window, 'getChromeUILanguage', () => 'en-ZA');
  test.speech.recognitionStubs.replace(
      window, 'webkitSpeechRecognition', function() {
        this.start = function() {
          test.speech.recognitionActiveCount++;
        };
        this.stop = function() {
          test.speech.recognitionActiveCount--;
        };
        this.abort = function() {
          test.speech.recognitionActiveCount--;
        };
        this.continuous = false;
        this.interimResults = false;
        this.maxAlternatives = 1;
        this.onerror = null;
        this.onnomatch = null;
        this.onend = null;
        this.onresult = null;
        this.onaudiostart = null;
        this.onspeechstart = null;
      });
  test.speech.recognitionStubs.replace(
      window, 'webkitSpeechRecognitionEvent', function() {
        this.results = null;
        this.resultIndex = -1;
      });
};

/**
 * Tests if the controller has the correct speech recognition settings.
 */
test.speech.testSpeechRecognitionInitSettings = function() {
  test.speech.recognitionStubs.reset();

  test.speech.initSpeech();
  assertFalse(speech.recognition_.continuous);
  assertEquals('en-ZA', speech.recognition_.lang);
  assertEquals(1, speech.recognition_.maxAlternatives);
  assertTrue(!!speech.recognition_);
  test.speech.validateInactive();
};

/**
 * Test that the initialization can only happen once.
 */
test.speech.testInitSuccessfullyChangesState = function() {
  assertEquals(speech.State_.UNINITIALIZED, speech.currentState_);
  test.speech.initSpeech();
  assertEquals(speech.State_.READY, speech.currentState_);
  assertThrows('not in UNINITIALIZED', () => test.speech.initSpeech());
};

/**
 * Test that the module doesn't cope with the Web Speech API missing.
 */
test.speech.testInitWithMissingSpeechRecognitionApiFails = function() {
  webkitSpeechRecognition = undefined;
  assertEquals(speech.State_.UNINITIALIZED, speech.currentState_);
  assertThrows(
      'SpeechRecognition is not a constructor', () => test.speech.initSpeech());
};

/**
 * Tests that with everything OK, clicking on the fakebox microphone icon
 * initiates speech.
 */
test.speech.testFakeboxClickStartsSpeechWithWorkingView = function() {
  test.speech.initSpeech();
  $(test.speech.FAKEBOX_MICROPHONE_ID).onclick(new MouseEvent('test'));

  assertEquals(speech.State_.STARTED, speech.currentState_);
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertTrue(speech.isRecognizing());
  assertTrue(test.speech.clock.isTimeoutSet(speech.idleTimer_));
  assertFalse(test.speech.clock.isTimeoutSet(speech.errorTimer_));
};

/**
 * Tests that with everything OK, focusing the Omnibox terminates speech.
 */
test.speech.testOmniboxFocusWithWorkingView = function() {
  test.speech.initSpeech();
  speech.start();

  assertEquals(speech.State_.STARTED, speech.currentState_);
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertTrue(speech.isRecognizing());
  assertTrue(test.speech.clock.isTimeoutSet(speech.idleTimer_));
  assertFalse(test.speech.clock.isTimeoutSet(speech.errorTimer_));

  speech.onOmniboxFocused();
  test.speech.validateInactive();
};

/**
 * Tests that with everything OK, focusing the Omnibox using keyboard navigation
 * does not terminate speech.
 */
test.speech.testOmniboxFocusWithKeyboardNavigationDoesNotAbort = function() {
  test.speech.initSpeech();
  const tabKey = new KeyboardEvent('test', {code: 'Tab'});
  speech.start();

  assertEquals(speech.State_.STARTED, speech.currentState_);
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertTrue(speech.isRecognizing());
  assertTrue(test.speech.clock.isTimeoutSet(speech.idleTimer_));
  assertFalse(test.speech.clock.isTimeoutSet(speech.errorTimer_));

  speech.onKeyDown(tabKey);
  speech.onOmniboxFocused();
  assertTrue(speech.isRecognizing());
};

/**
 * Tests that when the speech recognition interface is uninitialized,
 * clicking the speech input tool initializes it prior to starting the
 * interface.
 */
test.speech.testClickHandlingWithUnitializedSpeechRecognition = function() {
  test.speech.initSpeech();
  speech.recognition_ = undefined;
  assertEquals(speech.State_.READY, speech.currentState_);
  assertTrue(!speech.recognition_);
  speech.start();

  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertTrue(!!speech.recognition_);
};

/**
 * Tests that the view is notified when the speech recognition interface
 * starts the audio driver.
 */
test.speech.testHandleAudioStart = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);

  assertTrue('ready' in test.speech.viewState);
  assertTrue(speech.isRecognizing());
  assertEquals(1, test.speech.recognitionActiveCount);
  assertFalse(elementIsVisible($(test.speech.FAKEBOX_MICROPHONE_ID)));
};

/**
 * Tests that the view is notified when the speech recognition interface
 * starts receiving speech.
 */
test.speech.testHandleSpeechStart = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);

  assertTrue('receiving' in test.speech.viewState);
  assertTrue(speech.isRecognizing());
  assertEquals(1, test.speech.recognitionActiveCount);
  assertFalse(elementIsVisible($(test.speech.FAKEBOX_MICROPHONE_ID)));
};

/**
 * Tests the handling of a response received from the speech recognition
 * interface while the user is still speaking.
 */
test.speech.testHandleInterimSpeechResponse = function() {
  test.speech.initSpeech();
  const lowConfidenceText = 'low';
  const highConfidenceText = 'high';
  const viewText = highConfidenceText + lowConfidenceText;
  const responseEvent =
      test.speech.createInterimResponse(lowConfidenceText, highConfidenceText);

  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.recognition_.onresult(responseEvent);

  assertTrue(speech.isRecognizing());
  assertEquals(highConfidenceText, test.speech.viewState.final);
  assertEquals(viewText, test.speech.viewState.interim);
  assertEquals(highConfidenceText, speech.finalResult_);
  assertEquals(viewText, speech.interimResult_);
};

/**
 * Tests the handling of a response received from the speech recognition
 * interface after the user has finished speaking.
 */
test.speech.testHandleFinalSpeechResponse = function() {
  test.speech.initSpeech();
  const lowConfidenceText = 'low';
  const highConfidenceText = 'high';
  const responseEvent =
      test.speech.createFinalResponse(lowConfidenceText, highConfidenceText);

  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  // Handle a final transcript from the recognition API.
  speech.recognition_.onresult(responseEvent);

  test.speech.validateInactive();
  assertTrue(!!test.speech.locationUrl);
  assertEquals(
      test.speech.TEST_BASE_URL + 'search?q=high&gs_ivs=1',
      test.speech.locationUrl.href);

  // The speech results get reset after speech is stopped, which happens
  // just before query submission.
  assertEquals('', speech.interimResult_);
  assertEquals('', speech.finalResult_);
  assertEquals(highConfidenceText, test.speech.viewState.final);
  assertEquals(highConfidenceText, test.speech.viewState.interim);
};

/**
 * Tests the handling of user-interrupted speech recognition after an interim
 * speech recognition result is received.
 */
test.speech.testInterruptSpeechInputAfterInterimResult = function() {
  test.speech.initSpeech();
  const lowConfidenceText = 'low';
  const highConfidenceText = 'high';
  const responseEvent =
      test.speech.createInterimResponse(lowConfidenceText, highConfidenceText);

  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  // Handle an interim trancript from the recognition API.
  speech.recognition_.onresult(responseEvent);
  // The user interrupts speech.
  speech.stop();

  assertFalse(speech.isRecognizing());
  assertEquals('', speech.interimResult_);
  assertEquals('', speech.finalResult_);
  assertEquals(0, test.speech.recognitionActiveCount);

  // After stopping, results from the recognition API should be ignored.
  test.speech.createInterimResponse('result should', 'be ignored');
  speech.recognition_.onresult(responseEvent);

  assertFalse(speech.isRecognizing());
  assertEquals('', speech.interimResult_);
  assertEquals('', speech.finalResult_);
  assertEquals(0, test.speech.recognitionActiveCount);
};

/**
 * Tests the handling of user-interrupted speech recognition before any result
 * is received.
 */
test.speech.testInterruptSpeechInputBeforeResult = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.stop();

  test.speech.validateInactive();
};

/**
 * Tests that speech gets inactivated after an error is received and
 * an error timeout is fired.
 */
test.speech.testSpeechRecognitionErrorTimeout = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onerror({error: 'some-error'});

  assertFalse(speech.isRecognizing());
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertEquals(RecognitionError.OTHER, test.speech.viewState.error);

  test.speech.clock.advanceTime(2999);
  assertEquals(1, test.speech.viewActiveCount);

  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();
  test.speech.validateInactive();
};

/**
 * Tests that the proper error message is shown when the input ends before
 * speech is recognized, and that it gets hidden after a timeout.
 */
test.speech.testNoSpeechInput = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onend(null);

  assertFalse(speech.isRecognizing());
  assertEquals(RecognitionError.NO_SPEECH, test.speech.viewState.error);

  test.speech.clock.advanceTime(7999);
  assertEquals(1, test.speech.viewActiveCount);

  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();
  test.speech.validateInactive();
};

/**
 * Tests that recognition handlers stay initialized across successive
 * voice search invocations.
 */
test.speech.testRecognitionHandlersStayInitialized = function() {
  /**
   * Utility method for asserting whether recognition handlers are initialized.
   * @param {!boolean} expected True if the SpeechRecogniton object and all
   *     associated handlers are expected to be initialized, false if all are
   *     expected to be uninitialized.
   */
  function assertRecognitionHandlers(expected) {
    function assertRecognitionHandler(expected, handlerName) {
      assertEquals(expected, handlerName in speech.recognition_);
      assertEquals(expected, !!speech.recognition_[handlerName]);
    }

    assertEquals(expected, !!speech.recognition_);
    if (!!speech.recognition_) {
      assertRecognitionHandler(expected, 'onaudiostart');
      assertRecognitionHandler(expected, 'onend');
      assertRecognitionHandler(expected, 'onerror');
      assertRecognitionHandler(expected, 'onnomatch');
      assertRecognitionHandler(expected, 'onresult');
      assertRecognitionHandler(expected, 'onspeechstart');
    }
  }

  assertRecognitionHandlers(false);

  test.speech.initSpeech();
  assertRecognitionHandlers(true);

  speech.start();
  assertRecognitionHandlers(true);

  speech.recognition_.onaudiostart(null);
  // Stop.
  speech.recognition_.onend(null);
  assertRecognitionHandlers(true);
  assertFalse(speech.isRecognizing());
  assertEquals(RecognitionError.NO_SPEECH, test.speech.viewState.error);

  test.speech.clock.advanceTime(7999);
  assertEquals(1, test.speech.viewActiveCount);
  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();
  test.speech.validateInactive();
  assertRecognitionHandlers(true);

  speech.start();
  assertRecognitionHandlers(true);
};

/**
 * Tests starting and stopping the Speech Recognition API quickly
 * in succession.
 * Motivation: If |speech.start()| is called too soon after |speech.stop()|,
 * then the recognition interface hasn't yet reset and an error occurs.
 * In this case we need to hard-reset it and reissue the |recognition_.start()|
 * command.
 */
test.speech.testStopStartErrorHandling = function() {
  test.speech.recognitionStubs.reset();

  test.speech.initSpeech();
  speech.start();
  assertEquals(speech.State_.STARTED, speech.currentState_);
  assertTrue(speech.isRecognizing());

  speech.recognition_.onaudiostart(null);
  assertEquals(speech.State_.AUDIO_RECEIVED, speech.currentState_);
  assertTrue(speech.isRecognizing());

  speech.stop();
  assertEquals(speech.State_.READY, speech.currentState_);
  test.speech.validateInactive();

  speech.start();
  assertEquals(speech.State_.STARTED, speech.currentState_);
  assertTrue(speech.isRecognizing());

  speech.stop();
  assertEquals(speech.State_.READY, speech.currentState_);
  test.speech.validateInactive();
};

/**
 * Tests starting and stopping the Speech Recognition API quickly
 * in succession using keyboard shortcuts.
 */
test.speech.testStopStartKeyboardShortcutErrorHandling = function() {
  test.speech.recognitionStubs.reset();

  test.speech.initSpeech();
  const startShortcut = new KeyboardEvent(
      'start', {code: 'Period', shiftKey: true, ctrlKey: true, metaKey: true});
  const stopShortcut = new KeyboardEvent('stop', {code: 'Escape'});
  speech.onKeyDown(startShortcut);
  assertEquals(speech.State_.STARTED, speech.currentState_);

  speech.recognition_.onaudiostart(null);
  assertTrue(speech.isRecognizing());

  speech.onKeyDown(stopShortcut);
  assertEquals(speech.State_.READY, speech.currentState_);
  test.speech.validateInactive();

  speech.onKeyDown(startShortcut);
  assertTrue(speech.isRecognizing());
  assertEquals(speech.State_.STARTED, speech.currentState_);

  speech.onKeyDown(stopShortcut);
  assertEquals(speech.State_.READY, speech.currentState_);
  test.speech.validateInactive();
};

/**
 * Tests pressing Enter submits the speech query.
 */
test.speech.testEnterToSubmit = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.finalResult_ = 'test query';

  const keyEvent = new KeyboardEvent('testEvent', {'code': 'Enter'});
  speech.onKeyDown(keyEvent);

  test.speech.validateInactive();
  assertTrue(!!test.speech.locationUrl);
  assertEquals(
      test.speech.TEST_BASE_URL + 'search?q=test+query&gs_ivs=1',
      test.speech.locationUrl.href);

  assertEquals('', speech.interimResult_);
  assertEquals('', speech.finalResult_);
};

/**
 * Tests clicking to submit.
 */
test.speech.testClickToSubmit = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.finalResult_ = 'test query';
  speech.onClick_(
      /*submitQuery=*/true, /*shouldRetry=*/false, /*navigatingAway=*/false);

  test.speech.validateInactive();
  assertTrue(!!test.speech.locationUrl);
  assertEquals(
      test.speech.TEST_BASE_URL + 'search?q=test+query&gs_ivs=1',
      test.speech.locationUrl.href);

  assertEquals('', speech.interimResult_);
  assertEquals('', speech.finalResult_);
};

/**
 * Tests speech recognition is initiated with <CTRL> + <SHIFT> + <.>.
 */
test.speech.testKeyboardStartWithCtrl = function() {
  test.speech.initSpeech();

  const period = new KeyboardEvent('test', {'code': 'Period'});
  speech.onKeyDown(period);
  test.speech.validateInactive();

  const shiftPeriod =
      new KeyboardEvent('test', {code: 'Period', shiftKey: true});
  speech.onKeyDown(shiftPeriod);
  test.speech.validateInactive();

  const ctrlShiftPeriod = new KeyboardEvent(
      'test', {ctrlKey: true, code: 'Period', shiftKey: true});
  speech.onKeyDown(ctrlShiftPeriod);
  assertTrue(speech.isRecognizing());

  speech.onKeyDown(ctrlShiftPeriod);
  assertTrue(speech.isRecognizing());
};

/**
 * Tests speech recognition is initiated with <CMD> + <SHIFT> + <.> on Mac.
 */
test.speech.testKeyboardStartWithCmd = function() {
  test.speech.initSpeech();

  const period = new KeyboardEvent('test', {'code': 'Period'});
  speech.onKeyDown(period);
  test.speech.validateInactive();

  const shiftPeriod =
      new KeyboardEvent('test', {code: 'Period', shiftKey: true});
  speech.onKeyDown(shiftPeriod);
  test.speech.validateInactive();

  // Replace the user agent.
  let isUserAgentMac = false;
  test.speech.stubs.replace(speech, 'isUserAgentMac_', () => isUserAgentMac);

  // Set a non-Mac user agent.
  isUserAgentMac = false;
  const cmdShiftPeriod = new KeyboardEvent(
      'test', {code: 'Period', metaKey: true, shiftKey: true});
  speech.onKeyDown(cmdShiftPeriod);
  test.speech.validateInactive();

  // Set a Mac user agent.
  isUserAgentMac = true;
  speech.onKeyDown(cmdShiftPeriod);
  assertTrue(speech.isRecognizing());

  speech.onKeyDown(cmdShiftPeriod);
  assertTrue(speech.isRecognizing());
};

/**
 * Tests click to abort.
 */
test.speech.testClickToAbort = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.onClick_(
      /*submitQuery=*/false, /*shouldRetry=*/false, /*navigatingAway=*/false);

  test.speech.validateInactive();
};

/**
 * Tests click to retry when the interface is stopped restarts recognition.
 */
test.speech.testClickToRetryWhenStopped = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  // An onend event after onpeechstart forces an error and stops recognition.
  speech.recognition_.onend(null);
  assertEquals(speech.State_.STOPPED, speech.currentState_);

  speech.onClick_(
      /*submitQuery=*/false, /*shouldRetry=*/true, /*navigatingAway=*/false);
  assertTrue(speech.isRecognizing());
  assertEquals(speech.State_.STARTED, speech.currentState_);
};

/**
 * Tests click to retry (clicking the pulsing microphone button) when
 * the interface is not stopped stops recognition and hides the view.
 */
test.speech.testClickToRetryWhenNotStopped = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.onClick_(
      /*submitQuery=*/false, /*shouldRetry=*/true, /*navigatingAway=*/false);

  test.speech.validateInactive();
};

/**
 * Tests keyboard navigation on the support link.
 */
test.speech.testKeyboardNavigationOnSupportLink = function() {
  test.speech.initSpeech();
  const fakeKeyboardEvent = {
    target: {id: text.SUPPORT_LINK_ID},
    code: KEYCODE.ENTER,
    stopPropagation: () => {}
  };
  speech.start();

  speech.onKeyDown(fakeKeyboardEvent);
  assertEquals(text.SUPPORT_LINK_ID, test.speech.viewClickTarget.id);
};

/**
 * Tests keyboard navigation on the retry link.
 */
test.speech.testKeyboardNavigationOnRetryLink = function() {
  test.speech.initSpeech();
  const fakeKeyboardEvent = {
    target: {id: text.RETRY_LINK_ID},
    code: KEYCODE.SPACE,
    stopPropagation: () => {}
  };
  speech.start();

  speech.onKeyDown(fakeKeyboardEvent);
  assertEquals(text.RETRY_LINK_ID, test.speech.viewClickTarget.id);
};

/**
 * Tests keyboard navigation on the close button.
 */
test.speech.testKeyboardNavigationOnCloseButton = function() {
  test.speech.initSpeech();
  const fakeKeyboardEvent = {
    target: {id: view.CLOSE_BUTTON_ID},
    code: KEYCODE.NUMPAD_ENTER,
    stopPropagation: () => {}
  };
  speech.start();

  speech.onKeyDown(fakeKeyboardEvent);
  assertEquals(view.CLOSE_BUTTON_ID, test.speech.viewClickTarget.id);
};

/**
 * Tests that when the recognition API cannot match the input to text,
 * the proper error is displayed and the interface is closed, after a timeout.
 */
test.speech.testNoSpeechInputMatched = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.recognition_.onnomatch(null);

  assertFalse(speech.isRecognizing());
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertEquals(RecognitionError.NO_MATCH, test.speech.viewState.error);

  test.speech.clock.advanceTime(7999);
  assertEquals(1, test.speech.viewActiveCount);

  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();
  test.speech.validateInactive();
};

/**
 * Tests showing the proper error when there is no network connectivity.
 */
test.speech.testNetworkError = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onerror({error: 'network'});

  assertFalse(speech.isRecognizing());
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertEquals(RecognitionError.NETWORK, test.speech.viewState.error);

  test.speech.clock.advanceTime(7999);
  assertEquals(1, test.speech.viewActiveCount);

  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();
  test.speech.validateInactive();
};

/**
 * Tests showing the proper error when there is no network connectivity, after
 * interim results have been received.
 */
test.speech.testNetworkErrorAfterInterimResults = function() {
  test.speech.initSpeech();

  const lowConfidenceText = 'low';
  const highConfidenceText = 'high';
  const viewText = highConfidenceText + lowConfidenceText;
  const responseEvent =
      test.speech.createInterimResponse(lowConfidenceText, highConfidenceText);

  speech.start();
  speech.recognition_.onresult(responseEvent);
  speech.recognition_.onerror({error: 'network'});

  assertFalse(speech.isRecognizing());
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertEquals(RecognitionError.NETWORK, test.speech.viewState.error);

  test.speech.clock.advanceTime(7999);
  assertEquals(1, test.speech.viewActiveCount);

  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();
  test.speech.validateInactive();
};

/**
 * Tests showing the proper error when microphone permission is denied.
 */
test.speech.testPermissionError = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onerror({error: 'not-allowed'});

  assertFalse(speech.isRecognizing());
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);
  assertEquals(RecognitionError.NOT_ALLOWED, test.speech.viewState.error);

  test.speech.clock.advanceTime(7999);
  assertEquals(1, test.speech.viewActiveCount);

  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();
  test.speech.validateInactive();
};

/**
 * Tests that if no interactions occurs for some time during speech recognition,
 * the current high confidence speech results are submitted for search.
 */
test.speech.testIdleTimeoutWithConfidentSpeechResults = function() {
  test.speech.initSpeech();
  const lowConfidenceText = 'low';
  const highConfidenceText = 'high';
  const responseEvent =
      test.speech.createInterimResponse(lowConfidenceText, highConfidenceText);
  speech.reset_();

  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.recognition_.onresult(responseEvent);

  test.speech.clock.advanceTime(7999);
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);

  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();

  test.speech.validateInactive();
  assertTrue(!!test.speech.locationUrl);
  assertEquals(
      test.speech.TEST_BASE_URL + 'search?q=high&gs_ivs=1',
      test.speech.locationUrl.href);

  assertEquals('', speech.interimResult_);
  assertEquals('', speech.finalResult_);
  assertEquals(
      highConfidenceText + lowConfidenceText, test.speech.viewState.interim);
  assertEquals(highConfidenceText, test.speech.viewState.final);
};

/**
 * Tests that if no interactions occurs for some time during speech recognition
 * and no high confidence results have been received, the interface closes.
 */
test.speech.testIdleTimeoutWithNonConfidentSpeechResults = function() {
  test.speech.initSpeech();
  const lowConfidenceText = 'low';
  const highConfidenceText = '';
  const responseEvent =
      test.speech.createInterimResponse(lowConfidenceText, highConfidenceText);
  speech.reset_();

  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.recognition_.onresult(responseEvent);

  test.speech.clock.advanceTime(7999);
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);

  test.speech.clock.advanceTime(1);
  test.speech.clock.pendingTimeouts.shift().callback();
  assertEquals(speech.State_.ERROR_RECEIVED, speech.currentState_);
  assertEquals(1, test.speech.recognitionActiveCount);
  assertEquals(1, test.speech.viewActiveCount);

  // Wait for the error message timeout.
  test.speech.clock.advanceTime(3000);
  test.speech.clock.pendingTimeouts.shift().callback();
  test.speech.validateInactive();
  assertTrue(
      !test.speech.locationUrl ||
      !test.speech.locationUrl.href.startsWith(test.speech.TEST_BASE_URL));
};

/**
 * Tests that the query is properly encoded for use in a URL.
 */
test.speech.testQueryEncoding = function() {
  test.speech.initSpeech();
  speech.start();
  speech.recognition_.onaudiostart(null);
  speech.recognition_.onspeechstart(null);
  speech.finalResult_ = '🔍t&qôr 文字+weird*chär%?s?';
  speech.submitFinalResult_();

  assertTrue(!!test.speech.locationUrl);
  // To encode query: encodeURIComponent(queryText).replace(/%20/g, '+')
  assertEquals(
      test.speech.TEST_BASE_URL + 'search?q=%F0%9F%94%8Dt%26q%C3%B4r+' +
          '%E6%96%87%E5%AD%97%2Bweird*ch%C3%A4r%25%3Fs%3F&gs_ivs=1',
      test.speech.locationUrl.href);
};

// ***************************** HELPER FUNCTIONS *****************************
// These are used by the tests above.

/**
 * Utility function for initializing the speech module with mock objects
 * as parameters.
 */
test.speech.initSpeech = function() {
  speech.init(
      test.speech.TEST_BASE_URL, test.speech.TEST_STRINGS,
      $(test.speech.FAKEBOX_MICROPHONE_ID), test.speech.mockSearchBox);
};

/**
 * Resets the internal state of Voice Search and disables the speech
 * recognition interface.
 * @param {!HTMLElement} fakeboxMicrophoneElem Fakebox microphone icon element.
 * @param {!object} searchboxApiHandle SearchBox API handle.
 */
test.speech.unInitSpeech = function(fakeboxMicrophoneElem, searchboxApiHandle) {
  speech.reset_();
  speech.googleBaseUrl_ = null;
  speech.messages = {};
  speech.currentState_ = speech.State_.UNINITIALIZED;
  fakeboxMicrophoneElem.hidden = true;
  fakeboxMicrophoneElem.title = '';
  fakeboxMicrophoneElem.onclick = null;
  window.removeEventListener('keydown', speech.onKeyDown);
  searchboxApiHandle.onfocuschange = null;
  speech.recognition_ = null;
};

/**
 * Validates that speech is currently inactive and ready to start recognition.
 */
test.speech.validateInactive = function() {
  assertFalse(speech.isRecognizing());
  assertEquals(0, test.speech.recognitionActiveCount);
  assertEquals(0, test.speech.viewActiveCount);
  assertEquals('', speech.interimResult_);
  assertEquals('', speech.finalResult_);
  assertFalse(test.speech.clock.isTimeoutSet(speech.idleTimer_));
  assertFalse(test.speech.clock.isTimeoutSet(speech.errorTimer_));
};

/**
 * Generates a speech recognition response corresponding to one that is
 * generated after the speech recognition server has detected the end of voice
 * input and generated a final transcription.
 * @param {!string} interimText Low confidence transcript.
 * @param {!string} finalText High confidence transcript.
 * @return {webkitSpeechRecognitionEvent} The response event.
 */
test.speech.createFinalResponse = function(interimText, finalText) {
  const response = test.speech.createInterimResponse(interimText, finalText);
  response.results[response.resultIndex].isFinal = true;
  return response;
};

/**
 * Generates a speech recognition response corresponding to one that is
 * generated after the speech recognition server has generated a transcription
 * of the received input but the user hasn't finished speaking.
 * @param {!string} interimText Low confidence transcript.
 * @param {!string} finalText High confidence transcript.
 * @return {webkitSpeechRecognitionEvent} The response event.
 */
test.speech.createInterimResponse = function(interimText, finalText) {
  const response = new webkitSpeechRecognitionEvent();
  response.resultIndex = 0;
  response.results = [];
  response.results[0] = new test.speech.SpeechRecognitionResult();
  response.results[0][0] =
      test.speech.createSpeechRecognitionAlternative(finalText, 0.9);
  response.results[1] = new test.speech.SpeechRecognitionResult();
  response.results[1][0] =
      test.speech.createSpeechRecognitionAlternative(interimText, 0.1);
  return response;
};

/**
 * Generates a |SpeechRecognitionAlternative| that stores a speech
 * transcription and confidence level.
 * @param {!string} text The transcription text.
 * @param {!number} confidence The confidence level of the transcript.
 * @return {test.speech.SpeechRecognitionResultAlternative} The generated
 *     speech recognition transcription.
 */
test.speech.createSpeechRecognitionAlternative = function(text, confidence) {
  const alt = new test.speech.SpeechRecognitionResultAlternative();
  alt.transcript = text;
  alt.confidence = confidence;
  return alt;
};

/**
 * Mock of the |SpeechRecognitionResult| that stores
 * |SpeechRecognitionAlternative|-s and a boolean |isFinal| that indicates
 * whether this is a final or an interim result.
 * @constructor
 */
test.speech.SpeechRecognitionResult = function() {
  this.isFinal = false;
};

/**
 * Mock of the |SpeechRecognitionAlternative| that stores server-generated
 * speech transcripts.
 * @constructor
 */
test.speech.SpeechRecognitionResultAlternative = function() {
  this.transcript = '';
  this.confidence = -1;
};
