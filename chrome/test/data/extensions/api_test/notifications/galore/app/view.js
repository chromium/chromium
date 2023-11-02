// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mainWindow;
var sections = [];

var settings = {}
// Initial values.
settings.priority = "0";
settings.progress = 10;
settings.progress_sec = 2;
settings.progress_step = 20;

function onMainWindowClosed() {
  mainWindow = null;
  sections = [];
}

function createAppWindow(onLoad) {
  chrome.app.window.create('window.html', {
    id: 'window',
    defaultWidth: 440, minWidth: 440, maxWidth: 840,
    defaultHeight: 640, minHeight: 640, maxHeight: 940,
    hidden: true
  }, function(w) {
    mainWindow = w;
    mainWindow.contentWindow.onload = function() {
      setButtonHandlers();
      getElement("body").dataset.priority = settings.priority;
      onLoad();
    };
    mainWindow.onClosed.addListener(onMainWindowClosed)
  });
}

function resovleImageUrl(imageUrl, callback) {
  if (imageUrl.substr(0,4) != "http") {
    callback(imageUrl);
    return;
  }

  var xhr = new XMLHttpRequest();
  xhr.open("GET", imageUrl);
  xhr.responseType = "blob";
  xhr.onload = function() {
    callback(URL.createObjectURL(this.response));
  }
  xhr.send();
}

function addNotificationButton(sectionTitle,
                               buttonTitle,
                               iconUrl,
                               onClickHandler) {
  var button = getElement('#templates .notification').cloneNode(true);
  var image = button.querySelector('img');
  resovleImageUrl(iconUrl, function(url) { image.src = url });
  image.src = iconUrl;
  image.alt = buttonTitle;
  button.name = buttonTitle;
  button.onclick = onClickHandler;
  getSection(sectionTitle).appendChild(button);
}

function addProgressControl(sectionTitle) {
  var control = getElement('#templates .progress-control').cloneNode(true);
  getSection(sectionTitle).appendChild(control)

  var progress = control.querySelector('.progress');
  progress.id = "progress"
  progress.value = settings.progress;

  var progress_oneshot = control.querySelector('.progress-oneshot');
  progress_oneshot.id = 'progress-oneshot';
  progress_oneshot.checked = true;

  var progress_sec = control.querySelector('.progress-sec');
  progress_sec.id = "progress-sec"
  progress_sec.value = settings.progress_sec;

  var progress_step = control.querySelector('.progress-step');
  progress_step.id = "progress-step"
  progress_step.value = settings.progress_step;
}

function showWindow() {
  if (mainWindow)
    mainWindow.show();
}

function logEvent(message) {
  var event = getElement('#templates .event').cloneNode(true);
  event.textContent = message;
  getElement('#events').appendChild(event).scrollIntoView();
}

function logError(message) {
  var events = getElement('#events');
  var error = getElement('#templates .error').cloneNode(true);
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
  getElement("#recording-status").innerText = text;
}

function updateRecordingStatsDisplay(text) {
  getElement("#recording-stats").innerText = text;
}

function clearEvents() {
  var events = getElement('#events');
  while (events.lastChild)
    events.removeChild(events.lastChild);
}

function getSection(title) {
  sections[title] = (sections[title] || makeSection(title));
  return sections[title];
}

function makeSection(title) {
  var section = getElement('#templates .section').cloneNode(true);
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
  if (typeof elements === 'string')
    elements = mainWindow.contentWindow.document.querySelectorAll(elements);
  if (String(elements) === '[object NodeList]')
    elements = Array.prototype.slice.call(elements);
  return Array.isArray(elements) ? elements : [elements];
}
