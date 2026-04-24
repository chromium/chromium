// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const whiteboardValues = {
  topLeft: {
    x: 0.001,
    y: 0.0015,
  },
  topRight: {
    x: 0.9,
    y: 0.002,
  },
  bottomLeft: {
    x: 0.0018,
    y: 0.88,
  },
  bottomRight: {
    x: 0.85,
    y: 0.79,
  },
  aspectRatio: 1.76,
};

function getStateUninitialized() {
  chrome.mediaPerceptionPrivate.getState(
      chrome.test.callbackPass(function(state) {
        chrome.test.assertEq({ status: 'UNINITIALIZED' }, state);
      }));
}

function setStateRunning() {
  chrome.mediaPerceptionPrivate.setState({
    status: 'RUNNING',
    deviceContext: 'device_context',
    configuration: 'dummy_config',
    videoStreamParam: [
      {
        id: 'FaceDetection',
        width: 1280,
        height: 1920,
        frameRate: 30,
      },
    ],
    whiteboard: whiteboardValues,
  }, chrome.test.callbackPass(function(state) {
    chrome.test.assertEq('RUNNING', state.status);
    chrome.test.assertEq('dummy_config', state.configuration);
  }));
}

function getStateRunning() {
  chrome.mediaPerceptionPrivate.getState(
      chrome.test.callbackPass(function(state) {
        chrome.test.assertEq('RUNNING', state.status);
        chrome.test.assertEq('dummy_config', state.configuration);
      }));
}

function setStateUnsettable() {
  const error = 'Status can only be set to RUNNING, SUSPENDED, '
      + 'RESTARTING, or STOPPED.';
  chrome.mediaPerceptionPrivate.setState({
    status: 'UNINITIALIZED'
  }, chrome.test.callbackFail(error));
  chrome.mediaPerceptionPrivate.setState({
    status: 'STARTED'
  }, chrome.test.callbackFail(error));
}

function setStateSuspendedButWithDeviceContextFail() {
  const error = 'Only provide deviceContext with SetState RUNNING.';
  chrome.mediaPerceptionPrivate.setState({
    status: 'SUSPENDED',
    deviceContext: 'device_context'
  }, chrome.test.callbackFail(error));
}

function setStateSuspendedButWithConfigurationFail() {
  const error = 'Status must be RUNNING to set configuration.';
  chrome.mediaPerceptionPrivate.setState({
    status: 'SUSPENDED',
    configuration: 'dummy_config'
  }, chrome.test.callbackFail(error));
}

function setStateSuspendedButWithVideoStreamParamFail() {
  const error = 'SetState: status must be RUNNING to set videoStreamParam.';
  chrome.mediaPerceptionPrivate.setState({
    status: 'SUSPENDED',
    videoStreamParam: [
      {
        id: 'FaceDetection',
        width: 1280,
        height: 1920,
        frameRate: 30,
      },
    ],
  }, chrome.test.callbackFail(error));
}

function setStateSuspendedButWithWhiteboardFail() {
  const error = 'Status must be RUNNING to set whiteboard configuration.';
  chrome.mediaPerceptionPrivate.setState({
    status: 'SUSPENDED',
    whiteboard: whiteboardValues,
  }, chrome.test.callbackFail(error));
}

function setStateRestarted() {
  chrome.mediaPerceptionPrivate.setState({
    status: 'RESTARTING',
  }, chrome.test.callbackPass(function(state) {
    // Restarting the binary via Upstart results in it returning to a waiting
    // state (SUSPENDED) and ready to receive a request for setState RUNNING.
    chrome.test.assertEq('SUSPENDED', state.status);
  }));
}

function setStateRunningWithoutOptionalParameters() {
  chrome.mediaPerceptionPrivate.setState({
    status: 'RUNNING',
  }, chrome.test.callbackPass(function(state) {
    chrome.test.assertEq('RUNNING', state.status);
  }));
}

function setStateStopped() {
  chrome.mediaPerceptionPrivate.setState({
    status: 'STOPPED',
  }, chrome.test.callbackPass(function(state) {
    chrome.test.assertEq('STOPPED', state.status);
  }));
}

chrome.test.runTests([
    getStateUninitialized,
    setStateRunning,
    getStateRunning,
    setStateUnsettable,
    setStateSuspendedButWithDeviceContextFail,
    setStateSuspendedButWithConfigurationFail,
    setStateSuspendedButWithWhiteboardFail,
    setStateSuspendedButWithVideoStreamParamFail,
    setStateRestarted,
    setStateRunningWithoutOptionalParameters,
    setStateStopped]);
