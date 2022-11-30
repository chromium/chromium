// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * This class is a base class of each input method implementation.
 * @constructor
 */
var IMEBase = function() {};
IMEBase.prototype = {
  onActivate: function() {},
  onDeactivated: function() {},
  onFocus: function(context) {},
  onBlur: function(contextID) {},
  onInputContextUpdate: function(context) {},
  onKeyEvent: function(context, engine, keyData, requestID) { return false; },
  onCandidateClicked: function(candidateID, button) {},
  onMenuItemActivated: function(name) {},
  onSurroundingTextChanged: function(text, focus, anchor, offset) {},
  onReset: function(engineID) {}
};

/**
 * This class provides simple identity input methods.
 * @constructor
 **/
var IdentityIME = function() {};
IdentityIME.prototype = new IMEBase();

/**
 * This class provides an IME which capitalize given character.
 * @constructor
 */
var ToUpperIME = function() {};
ToUpperIME.prototype = new IMEBase();

/**
 * @param {Object} context A context object passed from input.ime.onFocus.
 * @param {string} engine An engine ID.
 * @param {Object} keyData A keyevent object passed from input.ime.onKeyEvent.
 * @param {string} requestID A unique ID for this key event.
 * @return {boolean} True on the key event is consumed.
 **/
ToUpperIME.prototype.onKeyEvent = function(context, engine, keyData, requestID)
{
  if (keyData.type == 'keydown' && /^[a-zA-Z]$/.test(keyData.key)) {
    chrome.input.ime.commitText({
      contextID: context.contextID,
      text: keyData.key.toUpperCase()
    }, function() {});
    return true;
  }
  return false;
};

/**
 * This class provide an IME which sneds message with API argument.
 * @constructor
 */
var APIArgumentIME = function() {};
APIArgumentIME.prototype = new IMEBase();
APIArgumentIME.prototype.nextRequestID_ = 1;

/**
 * @param {Object} context A context object passed from input.ime.onFocus.
 * @param {string} engine An engine ID.
 * @param {Object} keyData A keyevent object passed from input.ime.onKeyEvent.
 * @param {string} requestID A unique ID for this key event.
 * @return {boolean} True on the key event is consumed.
 **/
APIArgumentIME.prototype.onKeyEvent = function(context, engine, keyData,
    requestID)
{
  chrome.test.sendMessage('onKeyEvent:' +
                          (keyData.extensionId || '') + ':' +
                          (requestID === String(this.nextRequestID_)) + ':' +
                          keyData.type + ':' +
                          keyData.key + ':' +
                          keyData.code + ':' +
                          keyData.ctrlKey + ':' +
                          keyData.altKey + ':' +
                          keyData.altgrKey + ':' +
                          keyData.shiftKey + ':' +
                          keyData.capsLock);
  this.nextRequestID_++;
  return false;
};

chrome.input.ime.onAssistiveWindowButtonClicked.addListener(
    function(details) {
      chrome.test.sendMessage(
        `${details.buttonID} button in ${details.windowType} window clicked`);
    });

/**
 * This class listens the event from chrome.input.ime and forwards it to the
 * activated engine.
 * @constructor
 **/
var EngineBridge = function() {};
EngineBridge.prototype = {

  /**
   * Map from engineID to actual engine instance.
   * @type {Object}
   * @private
   **/
  engineInstance_: {},

  /**
   * A current active engineID.
   * @type {string}
   * @private
   **/
  activeEngine_: null,

  /**
   * A input context currently focused.
   * @type {string}
   * @private
   **/
  focusedContext_: null,

  /**
   * Called from chrome.input.ime.onActivate.
   * @private
   * @this EngineBridge
   **/
  onActivate_: function(engineID) {
    this.activeEngine_ = engineID;
    this.engineInstance_[engineID].onActivate();
    chrome.test.sendMessage('onActivate');
  },

  /**
   * Called from chrome.input.ime.onDeactivated.
   * @private
   * @this EngineBridge
   **/
  onDeactivated_: function(engineID) {
    if (this.engineInstance_[engineID])
      this.engineInstance_[engineID].onDeactivated();
    this.activeEngine_ = null;
    chrome.test.sendMessage('onDeactivated');
  },

  /**
   * Called from chrome.input.ime.onFocus.
   * @private
   * @this EngineBridge
   **/
  onFocus_: function(context) {
    this.focusedContext_ = context;
    if (this.activeEngine_)
      this.engineInstance_[this.activeEngine_].onFocus(context);
    chrome.test.sendMessage('onFocus:' +
                            context.type + ':' +
                            context.autoComplete + ':' +
                            context.autoCorrect + ':' +
                            context.spellCheck + ':' +
                            context.shouldDoLearning);
  },

  /**
   * Called from chrome.input.ime.onBlur.
   * @private
   * @this EngineBridge
   **/
  onBlur_: function(contextID) {
    if (this.activeEngine_)
      this.engineInstance_[this.activeEngine_].onBlur(contextID);
    this.focusedContext_ = null;
    chrome.test.sendMessage('onBlur');
  },

  /**
   * Called from chrome.input.ime.onInputContextUpdate.
   * @private
   * @this EngineBridge
   **/
  onInputContextUpdate_: function(context) {
    this.focusedContext_ = context;
    if (this.activeEngine_)
      this.engineInstance_[this.activeEngine_].onInputContextUpdate(context);
    chrome.test.sendMessage('onInputContextUpdate');
  },

  /**
   * Called from chrome.input.ime.onKeyEvent.
   * @private
   * @this EngineBridge
   * @return {boolean} True on the key event is consumed.
   **/
  onKeyEvent_: function(engineID, keyData, requestID) {
    chrome.test.sendMessage('onKeyEvent');
    if (this.engineInstance_[engineID])
      return this.engineInstance_[engineID].onKeyEvent(
          this.focusedContext_, this.activeEngine_, keyData, requestID);
    return false;
  },

  /**
   * Called from chrome.input.ime.onCandidateClicked.
   * @private
   * @this EngineBridge
   **/
  onCandidateClicked_: function(engineID, candidateID, button) {
    if (this.engineInstance_[engineID])
      this.engineInstance_[engineID].onCandidateClicked(candidateID, button);
    chrome.test.sendMessage('onCandidateClicked');
  },

  /**
   * Called from chrome.input.ime.onMenuItemActivated.
   * @private
   * @this EngineBridge
   **/
  onMenuItemActivated_: function(engineID, name) {
    this.engineInstance_[engineID].onMenuItemActivated(name);
    chrome.test.sendMessage('onMenuItemActivated');
  },

  /**
   * Called from chrome.input.ime.onSurroundingTextChanged.
   * @private
   * @this EngineBridge
   **/
  onSurroundingTextChanged_: function(engineID, object) {
    this.engineInstance_[engineID].onSurroundingTextChanged(
        object.text, object.focus, object.anchor, object.offset);
    chrome.test.sendMessage('onSurroundingTextChanged');
  },

  /**
   * Called from chrome.input.ime.onReset.
   * @private
   * @this EngineBridge
   **/
  onReset_: function(engineID) {
    this.engineInstance_[engineID].onReset(engineID);
    chrome.test.sendMessage('onReset');
  },

  /**
   * Add engine instance for |engineID|.
   * @this EngineBridge
   **/
  addEngine: function(engineID, engine) {
    this.engineInstance_[engineID] = engine;
  },

  /**
   * Returns active engine ID. Returns null if there is no active engine.
   * @this EngineBridge
   * @return {string} A string that identifies the engine.
   **/
  getActiveEngineID: function() {
    return this.activeEngine_;
  },

  /**
   * Returns currently focused context ID. Returns null if there is no focused
   * context.
   * @this EngineBridge
   * @return {strine} An string which identify the context.
   **/
  getFocusedContextID: function () {
    return this.focusedContext_;
  },

  /**
   * Initialize EngineBridge by binding with chrome event.
   * @this EngineBridge
   **/
  Initialize: function() {
    chrome.input.ime.onActivate.addListener(this.onActivate_.bind(this));
    chrome.input.ime.onDeactivated.addListener(this.onDeactivated_.bind(this));
    chrome.input.ime.onFocus.addListener(this.onFocus_.bind(this));
    chrome.input.ime.onBlur.addListener(this.onBlur_.bind(this));
    chrome.input.ime.onInputContextUpdate.addListener(
        this.onInputContextUpdate_.bind(this));
    chrome.input.ime.onKeyEvent.addListener(this. onKeyEvent_.bind(this));
    chrome.input.ime.onCandidateClicked.addListener(
        this.onCandidateClicked_.bind(this));
    chrome.input.ime.onMenuItemActivated.addListener(
        this.onMenuItemActivated_.bind(this));
    chrome.input.ime.onSurroundingTextChanged.addListener(
        this.onSurroundingTextChanged_.bind(this));
    chrome.input.ime.onReset.addListener(this.onReset_.bind(this));
  }
};

var engineBridge = new EngineBridge();
engineBridge.Initialize();
engineBridge.addEngine('IdentityIME', new IdentityIME());
engineBridge.addEngine('ToUpperIME', new ToUpperIME());
engineBridge.addEngine('APIArgumentIME', new APIArgumentIME());
chrome.test.sendMessage('ReadyToUseImeEvent');
