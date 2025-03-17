// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests preference.onChange API for an incognito split extension in case the
// extension's incognito instance is not expected to be brought up.

var hyperlinkAuditing = chrome.privacy.websites.hyperlinkAuditingEnabled;

function PreferenceChangeListener() {
  this.encounteredEvents = [];
  this.doneCallback_ = null;
  this.valueCallbacks_ = {};
}

PreferenceChangeListener.prototype.start = function(event) {
  var listener = this.onPrefChanged_.bind(this);

  event.addListener(listener);
  this.doneCallback_ = function() {
    event.removeListener(listener);
  }
};

PreferenceChangeListener.prototype.stop = function(callback) {
  if (this.doneCallback_) {
    this.doneCallback_();
    this.doneCallback_ = null;
  }
  chrome.test.assertEq([], this.encounteredEvents);
  chrome.test.assertEq({}, this.valueCallbacks_);
  callback();
};

PreferenceChangeListener.prototype.listenForValue = function(value, callback) {
  this.valueCallbacks_[value] = this.valueCallbacks_[value] || [];
  this.valueCallbacks_[value].push(callback);
};

PreferenceChangeListener.prototype.getAndClearEncounteredEvents = function() {
  var events = this.encounteredEvents;
  this.encounteredEvents = [];
  return events;
};

PreferenceChangeListener.prototype.onPrefChanged_ = function(pref) {
  this.encounteredEvents.push(pref);
  var callbacks = this.valueCallbacks_[pref.value];
  delete this.valueCallbacks_[pref.value];
  if (callbacks)
    callbacks.forEach(callback => callback());
};

var hyperlinkAuditingChangeListener = null;

// The incognito background is not expected to be run - send a message to the
// test runner, and bail out.
if (chrome.extension.inIncognitoContext) {
  chrome.test.sendMessage('incognito loaded');
} else {
  chrome.test.runTests([
    function setupPreferenceListener() {
      chrome.test.assertFalse(!!hyperlinkAuditingChangeListener);
      hyperlinkAuditingChangeListener = new PreferenceChangeListener();
      hyperlinkAuditingChangeListener.start(hyperlinkAuditing.onChange);
      chrome.test.succeed();
    },

    function getInitialValue() {
      hyperlinkAuditing.get({}, chrome.test.callbackPass(pref => {
        chrome.test.assertEq(
            {levelOfControl: 'controllable_by_this_extension', value: false},
            pref);
      }));
    },

    function listenForUserChange() {
      hyperlinkAuditingChangeListener.listenForValue(
          true, chrome.test.callbackPass(function() {
            var events =
                hyperlinkAuditingChangeListener.getAndClearEncounteredEvents();
            chrome.test.assertEq(events, [
              {levelOfControl: 'controllable_by_this_extension', value: true}
            ]);
          }));

      chrome.test.sendMessage('change pref value', chrome.test.callbackPass());
    },

    function changeDefault() {
      hyperlinkAuditingChangeListener.listenForValue(
          false, chrome.test.callbackPass(function() {
            var events =
                hyperlinkAuditingChangeListener.getAndClearEncounteredEvents();
            chrome.test.assertEq(events, [
              {value: false, levelOfControl: 'controlled_by_this_extension'}
            ]);
          }));

      hyperlinkAuditing.set({value: false}, chrome.test.callbackPass());
    },

    function changeIncognitoOnly() {
      hyperlinkAuditing.set(
          {value: true, scope: 'incognito_session_only'},
          chrome.test.callbackFail(
              'You do not have permission to access incognito preferences.'));
    },

    function clearControl() {
      hyperlinkAuditingChangeListener.listenForValue(
          true, chrome.test.callbackPass(function() {
            var events =
                hyperlinkAuditingChangeListener.getAndClearEncounteredEvents();
            chrome.test.assertEq(events, [
              {levelOfControl: 'controllable_by_this_extension', value: true}
            ]);
          }));

      hyperlinkAuditing.clear({}, chrome.test.callbackPass());
    },

    function stopPreferenceListener() {
      var listener = hyperlinkAuditingChangeListener;
      hyperlinkAuditingChangeListener = null;
      listener.stop(chrome.test.callbackPass());
    }
  ]);
}
