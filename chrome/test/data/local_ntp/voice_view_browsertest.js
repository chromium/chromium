// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests the view module of Voice Search on the local NTP.
 */

/**
 * Voice Search View module's object for test and setup functions.
 */
test.view = {};

/**
 * The set of textual strings for different states.
 * @const
 */
test.view.TEXT = {
  BLANK: '',
  ERROR: 'Error',
  SPEAK_NOW: 'Speak now',
  WAITING: 'Waiting...'
};

/**
 * Variable to indicate whether level animations are active.
 * @type {boolean}
 */
test.view.levelAnimationActive = false;

/**
 * The interim / low confidence speech recognition result element.
 * @type {string}
 */
test.view.interimText = '';

/**
 * The final / high confidence speech recognition result element.
 * @type {string}
 */
test.view.finalText = '';

/**
 * The state that is affected by the view.
 * @type {object}
 */
test.view.state = {};

/**
 * Utility to mock out object properties.
 * @type {Replacer}
 */
test.view.stubs = new Replacer();

/**
 * Set up the text DOM and test environment.
 */
test.view.setUp = function() {
  // If already initialized, reset the view module's state.
  if (!!view.background_) {
    view.hide();
    view.isNoMatchShown_ = false;
    view.isVisible_ = false;
  }

  test.view.levelAnimationActive = false;
  test.view.state = {};
  test.view.stubs.reset();

  setUpPage('voice-view-template');

  // Mock text area manipulating functions.
  test.view.stubs.replace(text, 'showInitializingMessage', function() {
    // Ignore the short timeout before showing "Waiting...".
    test.view.interimText = test.view.TEXT.WAITING;
    test.view.finalText = test.view.TEXT.BLANK;
  });
  test.view.stubs.replace(text, 'showReadyMessage', function() {
    test.view.interimText = test.view.TEXT.SPEAK_NOW;
    test.view.finalText = test.view.TEXT.BLANK;
  });
  test.view.stubs.replace(text, 'showErrorMessage', function() {
    test.view.interimText = test.view.TEXT.ERROR;
    test.view.finalText = test.view.TEXT.BLANK;
  });
  test.view.stubs.replace(text, 'updateTextArea', function(texti, textf = '') {
    test.view.interimText = texti;
    test.view.finalText = textf;
  });
  test.view.stubs.replace(text, 'clear', function() {
    test.view.interimText = test.view.TEXT.BLANK;
    test.view.finalText = test.view.TEXT.BLANK;
  });

  // Mock level animation state.
  test.view.stubs.replace(microphone, 'startInputAnimation', function() {
    test.view.levelAnimationActive = true;
  });
  test.view.stubs.replace(microphone, 'stopInputAnimation', function() {
    test.view.levelAnimationActive = false;
  });

  test.view.stubs.replace(speech, 'logEvent', function(event) {
    test.view.state.lastEvent = event;
  });

  view.init(function(shouldSubmit, shouldRetry, navigatingAway) {
    test.view.state.shouldSubmit = shouldSubmit;
    test.view.state.shouldRetry = shouldRetry;
    test.view.state.navigatingAway = navigatingAway;
  });
};

/**
 * Makes sure the view sets up with the correct settings.
 */
test.view.testInit = function() {
  test.view.assertViewInactive();
};

/**
 * Test showing the UI.
 */
test.view.testShowWithReadyElements = function() {
  view.show();

  test.view.assertViewActive(
      /*interim=*/test.view.TEXT.WAITING,
      /*final=*/test.view.TEXT.BLANK,
      /*containerClass=*/view.INACTIVE_CLASS_,
      /*levelAnimationActive=*/false);
};

/**
 * Test that trying to show the UI twice doesn't change the
 * view.
 */
test.view.testShowCalledTwice = function() {
  view.show();
  view.show();

  test.view.assertViewActive(
      /*interim=*/test.view.TEXT.WAITING,
      /*final=*/test.view.TEXT.BLANK,
      /*containerClass=*/view.INACTIVE_CLASS_,
      /*levelAnimationActive=*/false);
};

/**
 * Test that hiding the UI twice doesn't change the view.
 */
test.view.testHideCalledTwiceAfterShow = function() {
  view.show();
  view.hide();

  test.view.assertViewInactive();

  view.hide();

  test.view.assertViewInactive();
};

/**
 * Test showing the "Speak now" message does not show the pulsing animation.
 */
test.view.testAudioDeviceReady = function() {
  view.show();
  view.setReadyForSpeech();

  test.view.assertViewActive(
      /*interim=*/test.view.TEXT.SPEAK_NOW,
      /*final=*/test.view.TEXT.BLANK,
      /*containerClass=*/view.MICROPHONE_LISTENING_CLASS_,
      /*levelAnimationActive=*/false);
};

/**
 * Test that the listening text is not shown and the animations are not
 * started if the UI hasn't been started.
 */
test.view.testAudioDeviceListeningBeforeViewStart = function() {
  view.setReadyForSpeech();

  test.view.assertViewInactive();
};

/**
 * Test that the volume level animation is active after receiving speech
 * for the first time.
 */
test.view.testSpeechStartWithWorkingViews = function() {
  view.show();
  view.setReadyForSpeech();
  // The |onspeechstart| event fired.
  view.setReceivingSpeech();

  test.view.assertViewActive(
      /*interim=*/test.view.TEXT.SPEAK_NOW,
      /*final=*/test.view.TEXT.BLANK,
      /*containerClass=*/view.RECEIVING_SPEECH_CLASS_,
      /*levelAnimationActive=*/true);
};

/**
 * Test that the output text updates.
 */
test.view.testTextUpdateWithWorkingViews = function() {
  view.show();
  view.setReadyForSpeech();
  // The |onspeechstart| event did not fire, we got the results directly.
  view.updateSpeechResult('interim', 'final');

  test.view.assertViewActive(
      'interim', 'final',
      /*containerClass=*/view.RECEIVING_SPEECH_CLASS_,
      /*levelAnimationActive=*/true);
};

/**
 * Test that starting again after updating the output text doesn't change the
 * view state. This forces hide() to be called to restart the UI.
 */
test.view.testShowCalledAfterUpdate = function() {
  view.show();
  view.updateSpeechResult('interim', 'final');
  view.show();

  test.view.assertViewActive(
      'interim', 'final',
      /*containerClass=*/view.RECEIVING_SPEECH_CLASS_,
      /*levelAnimationActive=*/true);
};

/**
 * Test the typical flow for the view.
 */
test.view.testTypicalFlowWithWorkingViews = function() {
  test.view.assertViewInactive();
  view.show();
  view.setReadyForSpeech();
  view.setReceivingSpeech();
  view.updateSpeechResult('interim1', 'final1');

  test.view.assertViewActive(
      'interim1', 'final1',
      /*containerClass=*/view.RECEIVING_SPEECH_CLASS_,
      /*levelAnimationActive=*/true);

  view.updateSpeechResult('interim2', 'final2');

  test.view.assertViewActive(
      'interim2', 'final2',
      /*containerClass=*/view.RECEIVING_SPEECH_CLASS_,
      /*levelAnimationActive=*/true);

  view.hide();

  test.view.assertViewInactive();
};

/**
 * Test hiding the UI after showing it.
 */
test.view.testStopAfterStart = function() {
  view.show();
  view.hide();

  test.view.assertViewInactive();
};

/**
 * Test hiding the UI after audio start.
 */
test.view.testHideAfterAudioStart = function() {
  view.show();
  view.setReadyForSpeech();
  view.hide();

  test.view.assertViewInactive();
};

/**
 * Test hiding the UI after showing speech results.
 */
test.view.testHideAfterSpeechTranscriptReceived = function() {
  view.show();
  view.setReadyForSpeech();
  view.setReceivingSpeech();
  view.updateSpeechResult('interim', 'final');
  view.hide();

  test.view.assertViewInactive();
};

/**
 * Test hiding the UI after speech start.
 */
test.view.testHideAfterSpeechStart = function() {
  view.show();
  view.setReadyForSpeech();
  view.setReceivingSpeech();
  view.hide();

  test.view.assertViewInactive();
};

/**
 * Test that clicking the microphone button when the "Didn't get that" message
 * has been shown retries voice search.
 */
test.view.testClickMicButtonWithNoMatch = function() {
  view.show();
  assertFalse(view.isNoMatchShown_);
  // Show: "Didn't get that. Try again"
  view.showError(RecognitionError.NO_MATCH);
  assertTrue(view.isNoMatchShown_);

  const fakeClickEvent = {target: {id: microphone.RED_BUTTON_ID}};
  view.onWindowClick_(fakeClickEvent);

  assertTrue(test.view.state.shouldRetry);
  assertFalse(test.view.state.shouldSubmit);
  assertFalse(test.view.state.navigatingAway);
  assertEquals(LOG_TYPE.ACTION_TRY_AGAIN_MIC_BUTTON, test.view.state.lastEvent);
};

/**
 * Test that clicking the retry link when the "Didn't get that" message
 * has been shown retries voice search.
 */
test.view.testClickTryAgainLinkWithNoMatch = function() {
  view.show();
  assertFalse(view.isNoMatchShown_);
  // Show: "Didn't get that. Try again"
  view.showError(RecognitionError.NO_MATCH);
  assertTrue(view.isNoMatchShown_);

  const fakeClickEvent = {target: {id: text.RETRY_LINK_ID}};
  view.onWindowClick_(fakeClickEvent);

  assertTrue(test.view.state.shouldRetry);
  assertFalse(test.view.state.shouldSubmit);
  assertFalse(test.view.state.navigatingAway);
  assertEquals(LOG_TYPE.ACTION_TRY_AGAIN_LINK, test.view.state.lastEvent);
};

/**
 * Test that clicking on the microphone with results present submits the query.
 */
test.view.testClickMicButtonWithResults = function() {
  view.show();
  view.setReadyForSpeech();
  view.updateSpeechResult('interim', 'final');

  const fakeClickEvent = {target: {id: microphone.RED_BUTTON_ID}};
  view.onWindowClick_(fakeClickEvent);

  assertFalse(test.view.state.shouldRetry);
  assertTrue(test.view.state.shouldSubmit);
  assertFalse(test.view.state.navigatingAway);
  assertTrue(!test.view.state.lastEvent);
};

/**
 * Test that clicking on the microphone after showing results that then changed
 * into a "Didn't get that" error retries voice search.
 */
test.view.testClickMicButtonWithNoMatchAfterResults = function() {
  view.show();
  view.setReadyForSpeech();
  view.updateSpeechResult('interim');
  assertFalse(view.isNoMatchShown_);
  // Show: "Didn't get that. Try again"
  view.showError(RecognitionError.NO_MATCH);
  assertTrue(view.isNoMatchShown_);

  const fakeClickEvent = {target: {id: microphone.RED_BUTTON_ID}};
  view.onWindowClick_(fakeClickEvent);

  assertTrue(test.view.state.shouldRetry);
  assertFalse(test.view.state.shouldSubmit);
  assertFalse(test.view.state.navigatingAway);
  assertEquals(LOG_TYPE.ACTION_TRY_AGAIN_MIC_BUTTON, test.view.state.lastEvent);
};

/**
 * Test that clicking on the background with results present hides the view.
 */
test.view.testClickBackgroundWithResults = function() {
  view.show();
  view.setReadyForSpeech();
  view.updateSpeechResult('interim', 'final');

  const fakeClickEvent = {target: {id: view.BACKGROUND_ID_}};
  view.onWindowClick_(fakeClickEvent);

  assertFalse(test.view.state.shouldRetry);
  assertFalse(test.view.state.shouldSubmit);
  assertFalse(test.view.state.navigatingAway);
};

/**
 * Test that clicking on the close button with results present hides the view.
 */
test.view.testClickCloseButtonWithResults = function() {
  view.show();
  view.setReadyForSpeech();
  view.updateSpeechResult('interim', 'final');

  const fakeClickEvent = {target: {id: 'voice-close-button'}};
  view.onWindowClick_(fakeClickEvent);

  assertFalse(test.view.state.shouldRetry);
  assertFalse(test.view.state.shouldSubmit);
  assertFalse(test.view.state.navigatingAway);
};

/**
 * Test that clicking the support link does not change the UI, and waits
 * for navigation.
 */
test.view.testClickSupportLinkWithError = function() {
  view.show();
  // Show: "Please check your microphone and audio levels. Learn more"
  view.showError(RecognitionError.NO_SPEECH);

  const fakeClickEvent = {target: {id: text.SUPPORT_LINK_ID}};
  view.onWindowClick_(fakeClickEvent);

  assertFalse(test.view.state.shouldRetry);
  assertFalse(test.view.state.shouldSubmit);
  assertTrue(test.view.state.navigatingAway);
  assertEquals(LOG_TYPE.ACTION_SUPPORT_LINK_CLICKED, test.view.state.lastEvent);
};

/**
 * Test that showing an unknown error message is handled gracefully.
 */
test.view.testShowingUnknownErrorDoesNotProduceAnError = function() {
  view.show();
  view.showError(RecognitionError.OTHER);

  test.view.assertViewActive(
      /*interim=*/test.view.TEXT.ERROR,
      /*final=*/test.view.TEXT.BLANK,
      /*containerClass=*/view.ERROR_RECEIVED_CLASS_,
      /*levelAnimationActive=*/false);
};

// ***************************** HELPER FUNCTIONS *****************************
// Helper functions used in tests.

/**
 * Tests to make sure no components of the view are active.
 */
test.view.assertViewInactive = function() {
  assertFalse(view.isVisible_);
  assertFalse(view.isNoMatchShown_);
  assertEquals(view.OVERLAY_HIDDEN_CLASS_, view.background_.className);

  assertEquals(test.view.TEXT.BLANK, test.view.interimText);
  assertEquals(test.view.TEXT.BLANK, test.view.finalText);
  assertEquals(view.INACTIVE_CLASS_, view.container_.className);
  assertFalse(test.view.levelAnimationActive);
};

/**
 * Tests to make sure the all components of the view are working correctly.
 * @param {string} interim The expected low confidence (interim) text string.
 * @param {string} final The expected high confidence (final) text string.
 * @param {string} containerClass The expected CSS class of the container
 *     used to position the microphone and text output area.
 * @param {!boolean} levelAnimationActive The expected state of the level
 *     animation.
 */
test.view.assertViewActive = function(
    interim, final, containerClass, levelAnimationActive) {
  assertTrue(view.isVisible_);
  assertEquals(view.OVERLAY_CLASS_, view.background_.className);

  assertEquals(interim, test.view.interimText);
  assertEquals(final, test.view.finalText);
  assertEquals(containerClass, view.container_.className);
  assertEquals(levelAnimationActive, test.view.levelAnimationActive);
};
