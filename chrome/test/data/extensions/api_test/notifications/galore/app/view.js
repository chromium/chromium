// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let mainWindow;
let sections = [];

const settings = {};
// Initial values.
settings.priority = '0';
settings.progress = 10;
settings.progressSec = 2;
settings.progressStep = 20;

function onMainWindowClosed() {
  mainWindow = null;
  sections = [];
}

function createAppWindow(onLoad) {
  chrome.app.window.create(
      'window.html', {
        id: 'window',
        defaultWidth: 440,
        minWidth: 440,
        maxWidth: 840,
        defaultHeight: 640,
        minHeight: 640,
        maxHeight: 940,
        hidden: true,
      },
      function(w) {
        mainWindow = w;
        mainWindow.contentWindow.onload = function() {
          setButtonHandlers();
          getElement('body').dataset.priority = settings.priority;
          onLoad();
        };
        mainWindow.onClosed.addListener(onMainWindowClosed);
      });
}

function resovleImageUrl(imageUrl, callback) {
  if (imageUrl.substr(0, 4) != 'http') {
    callback(imageUrl);
    return;
  }

  const xhr = new XMLHttpRequest();
  xhr.open('GET', imageUrl);
  xhr.responseType = 'blob';
  xhr.onload = function() {
    callback(URL.createObjectURL(this.response));
  };
  xhr.send();
}

function addNotificationButton(
    sectionTitle, buttonTitle, iconUrl, onClickHandler) {
  const button = getElement('#templates .notification').cloneNode(true);
  const image = button.querySelector('img');
  resovleImageUrl(iconUrl, function(url) {
    image.src = url;
  });
  image.src = iconUrl;
  image.alt = buttonTitle;
  button.name = buttonTitle;
  button.onclick = onClickHandler;
  getSection(sectionTitle).appendChild(button);
}

function addProgressControl(sectionTitle) {
  const control = getElement('#templates .progress-control').cloneNode(true);
  getSection(sectionTitle).appendChild(control);

  const progress = control.querySelector('.progress');
  progress.id = 'progress';
  progress.value = settings.progress;

  const progressOneshot = control.querySelector('.progress-oneshot');
  progressOneshot.id = 'progress-oneshot';
  progressOneshot.checked = true;

  const progressSec = control.querySelector('.progress-sec');
  progressSec.id = 'progress-sec';
  progressSec.value = settings.progressSec;

  const progressStep = control.querySelector('.progress-step');
  progressStep.id = 'progress-step';
  progressStep.value = settings.progressStep;
}

function showWindow() {
  if (mainWindow) {
    mainWindow.show();
  }
}

function logEvent(message) {
  const event = getElement('#templates .event').cloneNode(true);
  event.textContent = message;
  getElement('#events').appendChild(event).scrollIntoView();
}

function logError(message) {
  const events = getElement('#events');
  const error = getElement('#templates .error').cloneNode(true);
  error.textContent = message;
  events.appendChild(error).scrollIntoView();
}

function setButtonHandlers() {
  setButtonAction('#clear-events', clearEvents);
  setButtonAction('#record', onRecord);
  setButtonAction('#pause', onPause);
  setButtonAction('#stop', onStop);
  setButtonAction('#play', onPlay);
}

function setRecorderStatusText(text) {
  getElement('#recording-status').innerText = text;
}

function updateRecordingStatsDisplay(text) {
  getElement('#recording-stats').innerText = text;
}

function clearEvents() {
  const events = getElement('#events');
  while (events.lastChild) {
    events.removeChild(events.lastChild);
  }
}

function getSection(title) {
  sections[title] = (sections[title] || makeSection(title));
  return sections[title];
}

function makeSection(title) {
  const section = getElement('#templates .section').cloneNode(true);
  section.querySelector('span').textContent = title;
  return getElement('#notifications').appendChild(section);
}

function setButtonAction(elements, action) {
  getElements(elements).forEach(function(element) {
    element.onclick = action;
  });
}

function getElement(element) {
  return getElements(element)[0];
}

function getElements(elements) {
  if (typeof elements === 'string') {
    elements = mainWindow.contentWindow.document.querySelectorAll(elements);
  }
  if (String(elements) === '[object NodeList]') {
    elements = Array.prototype.slice.call(elements);
  }
  return Array.isArray(elements) ? elements : [elements];
}
