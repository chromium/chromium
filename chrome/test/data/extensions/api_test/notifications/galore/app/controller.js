// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const STOPPED = 'Stopped';
const RECORDING = 'Recording';
const PAUSED_RECORDING = 'Recording Paused';
const PAUSED_PLAYING = 'Playing Paused';
const PLAYING = 'Playing';

let recordingState = STOPPED;

// Timestamp when current segment started.
let segmentStart;
// Segment duration accumulated before pause button was hit.
let pausedDuration;
// The array of segments, with delay and action.
let recordingList;
// When this timer fires, the next segment from recordingList should be played.
let playingTimer;
let currentSegmentIndex;
// A set of web Notifications - used to delete them during playback by id.
let webNotifications = {};

const recorderButtons = ['play', 'record', 'pause', 'stop'];
const recorderButtonStates = [
  {state: STOPPED, enabled: 'play record'},
  {state: RECORDING, enabled: 'pause stop'},
  {state: PAUSED_RECORDING, enabled: 'record stop'},
  {state: PAUSED_PLAYING, enabled: 'play stop'},
  {state: PLAYING, enabled: 'pause stop'},
];

// This function forms 2 selector lists - one that includes enabled buttons
// and one that includes disabled ones. Then it applies "disabled" attribute to
// corresponding sets of buttons.
function updateButtonsState() {
  recorderButtonStates.map(function(entry) {
    if (entry.state != recordingState) {
      return;
    }
    // Found entry with current recorder state. Now compute the sets
    // of enabled/disabled buttons.
    // Copy a list of all buttons.
    const disabled = recorderButtons.slice(0);
    // Get an array of enabled buttons for the state.
    const enabled = entry.enabled.split(' ');
    // Remove enabled buttons from disabled list, prefix them with "#" so they
    // form proper id selectors.
    for (let i = 0; i < enabled.length; i++) {
      disabled.splice(disabled.indexOf(enabled[i]), 1);
      enabled[i] = `#${enabled[i]}`;
    }
    // Prefix remaining disabled ids to form proper id selectors.
    for (let i = 0; i < disabled.length; i++) {
      disabled[i] = `#${disabled[i]}`;
    }
    getElements(disabled.join(', ')).forEach(function(element) {
      element.setAttribute('disabled', 'true');
    });
    getElements(enabled.join(', ')).forEach(function(element) {
      element.removeAttribute('disabled');
    });
  });
}


function setRecordingState(newState) {
  setRecorderStatusText(newState);
  recordingState = newState;
  updateButtonsState();
}

function updateRecordingStats(context) {
  let length = 0;
  let segmentCnt = 0;
  recordingList.slice(currentSegmentIndex).forEach(function(segment) {
    length += segment.delay || 0;
    segmentCnt++;
  });
  updateRecordingStatsDisplay(
      context + ': ' + (segmentCnt - 1) + ' segments, ' +
      Math.floor(length / 1000) + ' seconds.');
}

function loadRecording() {
  chrome.storage.local.get('recording', function(items) {
    recordingList = JSON.parse(items['recording'] || '[]');
    setRecordingState(STOPPED);
    updateRecordingStats('Loaded record');
  });
}

function finalizeRecording() {
  chrome.storage.local.set({recording: JSON.stringify(recordingList)});
  updateRecordingStats('Recorded');
}

function setPreviousSegmentDuration() {
  const now = new Date().getTime();
  const delay = now - segmentStart;
  segmentStart = now;
  recordingList[recordingList.length - 1].delay = delay;
}

function cloneOptions(obj) {
  if (obj == null || typeof (obj) !== 'object') {
    return obj;
  }

  const temp = {};
  for (const key in obj) {
    temp[key] = cloneOptions(obj[key]);
  }
  return temp;
}

function recordCreate(kind, id, options) {
  if (recordingState != RECORDING) {
    return;
  }
  setPreviousSegmentDuration();
  recordingList.push(
      {type: 'create', kind: kind, id: id, options: cloneOptions(options)});
  updateRecordingStats('Recording');
}

function recordUpdate(kind, id, options) {
  if (recordingState != RECORDING) {
    return;
  }
  setPreviousSegmentDuration();
  recordingList.push(
      {type: 'update', kind: kind, id: id, options: cloneOptions(options)});
  updateRecordingStats('Recording');
}

function recordDelete(kind, id) {
  if (recordingState != RECORDING) {
    return;
  }
  setPreviousSegmentDuration();
  recordingList.push({type: 'delete', kind: kind, id: id});
  updateRecordingStats('Recording');
}

function startPlaying() {
  if (recordingList.length < 2) {
    return false;
  }

  setRecordingState(PLAYING);

  if (playingTimer) {
    clearTimeout(playingTimer);
  }

  webNotifications = {};
  currentSegmentIndex = 0;
  playingTimer =
      setTimeout(playNextSegment, recordingList[currentSegmentIndex].delay);
  updateRecordingStats('Playing');
}

function playNextSegment() {
  currentSegmentIndex++;
  const segment = recordingList[currentSegmentIndex];
  if (!segment) {
    stopPlaying();
    return;
  }

  if (segment.type == 'create') {
    createNotificationForPlay(segment.kind, segment.id, segment.options);
  } else if (segment.type == 'update') {
    updateNotificationForPlay(segment.kind, segment.id, segment.options);
  } else {  // type == 'delete'
    deleteNotificationForPlay(segment.kind, segment.id);
  }
  playingTimer =
      setTimeout(playNextSegment, recordingList[currentSegmentIndex].delay);
  segmentStart = new Date().getTime();
  updateRecordingStats('Playing');
}

function deleteNotificationForPlay(kind, id) {
  if (kind == 'web') {
    webNotifications[id].close();
  } else {
    chrome.notifications.clear(id, function(wasClosed) {
      // nothing to do
    });
  }
}

function createNotificationForPlay(kind, id, options) {
  if (kind == 'web') {
    webNotifications[id] = createWebNotification(id, options);
  } else {
    const type = options.type;
    const priority = options.priority;
    createRichNotification(id, type, priority, options);
  }
}

function updateNotificationForPlay(kind, id, options) {
  if (kind == 'web') {
    // TODO: implement update.
  } else {
    const type = options.type;
    const priority = options.priority;
    updateRichNotification(id, type, priority, options);
  }
}

function stopPlaying() {
  currentSegmentIndex = 0;
  clearTimeout(playingTimer);
  updateRecordingStats('Record');
  setRecordingState(STOPPED);
}

function pausePlaying() {
  clearTimeout(playingTimer);
  pausedDuration = new Date().getTime() - segmentStart;
  setRecordingState(PAUSED_PLAYING);
}

function unpausePlaying() {
  let remainingInSegment =
      recordingList[currentSegmentIndex].delay - pausedDuration;
  if (remainingInSegment < 0) {
    remainingInSegment = 0;
  }
  playingTimer = setTimeout(playNextSegment, remainingInSegment);
  segmentStart = new Date().getTime() - pausedDuration;
}

function onRecord() {
  if (recordingState == STOPPED) {
    segmentStart = new Date().getTime();
    pausedDuration = 0;
    // This item is only needed to keep a duration of the delay between start
    // and first action.
    recordingList = [{type: 'start'}];
  } else if (recordingState == PAUSED_RECORDING) {
    segmentStart = new Date().getTime() - pausedDuration;
    pausedDuration = 0;
  } else {
    return;
  }
  updateRecordingStats('Recording');
  setRecordingState(RECORDING);
}

function pauseRecording() {
  pausedDuration = new Date().getTime() - segmentStart;
  segmentStart = 0;
  setRecordingState(PAUSED_RECORDING);
}

function onPause() {
  if (recordingState == RECORDING) {
    pauseRecording();
  } else if (recordingState == PLAYING) {
    pausePlaying();
  } else {
    return;
  }
}

function onStop() {
  switch (recordingState) {
    case PAUSED_RECORDING:
      segmentStart = new Date().getTime() - pausedDuration;
      // fall through
    case RECORDING:
      finalizeRecording();
      break;
    case PLAYING:
    case PAUSED_PLAYING:
      stopPlaying();
      break;
  }
  setRecordingState(STOPPED);
}

function onPlay() {
  if (recordingState == STOPPED) {
    if (!startPlaying()) {
      return;
    }
  } else if (recordingState == PAUSED_PLAYING) {
    unpausePlaying();
  }
  setRecordingState(PLAYING);
}

function createWindow() {
  chrome.storage.local.get('settings', onSettingsFetched);
}

function onSettingsFetched(items) {
  settings = items.settings || settings;
  const request = new XMLHttpRequest();
  const source = '/data/data.json';
  request.open('GET', source, true);
  request.responseType = 'text';
  request.onload = onDataFetched;
  request.send();
}

function onDataFetched() {
  const data = JSON.parse(this.response);
  createAppWindow(function() {
    // Create notification buttons.
    data.forEach(function(section) {
      const type = section.notificationType;
      if (type == 'progress') {
        addProgressControl(section.sectionName);
      }
      (section.notificationOptions || []).forEach(function(options) {
        addNotificationButton(
            section.sectionName, options.title, options.iconUrl, function() {
              createNotification(type, options);
            });
      });
    });
    loadRecording();
    addListeners();
    showWindow();
  });
}

function onSettingsChange(settings) {
  chrome.storage.local.set({settings: settings});
}

function scheduleNextProgress(id, priority, options, progress, step, timeout) {
  let newProgress = progress + step;
  if (newProgress > 100) {
    newProgress = 100;
  }
  setTimeout(
      nextProgress(id, priority, options, newProgress, step, timeout), timeout);
}

function nextProgress(id, priority, options, progress, step, timeout) {
  return (function() {
    options['progress'] = progress;
    updateRichNotification(id, 'progress', priority, options);
    if (progress >= 100) {
      return;
    }
    scheduleNextProgress(id, priority, options, progress, step, timeout);
  });
}

function createNotification(type, options) {
  const id = getNextId();
  const priority = Number(settings.priority || 0);
  if (type == 'web') {
    createWebNotification(id, options);
  } else {
    if (type == 'progress') {
      if (getElement('#progress-oneshot').checked) {
        options['progress'] = Number(getElement('#progress').value);
      } else {
        const step = Number(getElement('#progress-step').value);
        options['progress'] = step;
        if (options['progress'] < 100) {
          scheduleNextProgress(
              id, priority, options, options['progress'], step,
              Number(getElement('#progress-sec').value) * 1000);
        }
      }
    }
    createRichNotification(id, type, priority, options);
  }
}

function createWebNotification(id, options) {
  const iconUrl = options.iconUrl;
  const title = options.title;
  const message = options.message;
  const n = new Notification(title, {
    body: message,
    icon: iconUrl,
    tag: id,
  });
  n.onshow = function() {
    logEvent(`WebNotification #${id}: onshow`);
  };
  n.onclick = function() {
    logEvent(`WebNotification #${id}: onclick`);
  };
  n.onclose = function() {
    logEvent(`WebNotification #${id}: onclose`);
    recordDelete('web', id);
  };
  logMethodCall('created', 'Web', id, 'title: "' + title + '"');
  recordCreate('web', id, options);
  return n;
}

function createRichNotification(id, type, priority, options) {
  options['type'] = type;
  options['priority'] = priority;
  chrome.notifications.create(id, options, function() {
    const argument1 = 'type: "' + type + '"';
    const argument2 = `priority: ${priority}`;
    const argument3 = 'title: "' + options.title + '"';
    logMethodCall('created', 'Rich', id, argument1, argument2, argument3);
  });
  recordCreate('rich', id, options);
}

function updateRichNotification(id, type, priority, options) {
  options['type'] = type;
  options['priority'] = priority;
  chrome.notifications.update(id, options, function() {
    const argument1 = 'type: "' + type + '"';
    const argument2 = `priority: ${priority}`;
    const argument3 = 'title: "' + options.title + '"';
    logMethodCall('updated', 'Rich', id, argument1, argument2, argument3);
  });
  recordUpdate('rich', id, options);
}

let counter = 0;
function getNextId() {
  return String(counter++);
}

function addListeners() {
  chrome.notifications.onClosed.addListener(onClosed);
  chrome.notifications.onClicked.addListener(onClicked);
  chrome.notifications.onButtonClicked.addListener(onButtonClicked);
}

function logMethodCall(method, kind, id, varArgs) {
  logEvent(
      `${kind} Notification #${id}: ${method} ` +
      `(${Array.prototype.slice.call(arguments, 2).join(', ')})`);
}

function onClosed(id) {
  logEvent(`Notification #${id}: onClosed`);
  recordDelete('rich', id);
}

function onClicked(id) {
  logEvent(`Notification #${id}: onClicked`);
}

function onButtonClicked(id, index) {
  logEvent(`Notification #${id}: onButtonClicked, btn: ${index}`);
}
