// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Action = {
  VERIFY_APPINSTALLED: 'verify_appinstalled',
  VERIFY_APPINSTALLED_STASH_EVENT: 'verify_appinstalled_stash_event',
  VERIFY_PROMPT_APPINSTALLED: 'verify_prompt_appinstalled',
  VERIFY_BEFOREINSTALLPROMPT: 'verify_beforeinstallprompt',
  CALL_PROMPT_DELAYED: 'call_prompt_delayed',
  CALL_PROMPT_IN_HANDLER: 'call_prompt_in_handler',
  CALL_PROMPT_NO_USERCHOICE: 'call_prompt_no_userchoice',
  CALL_STASHED_PROMPT_ON_CLICK: 'call_stashed_prompt_on_click',
  CALL_STASHED_PROMPT_ON_CLICK_VERIFY_APPINSTALLED:
      'call_stashed_prompt_on_click_verify_appinstalled',
  CANCEL_PROMPT_AND_NAVIGATE: 'cancel_prompt_and_navigate',
  CANCEL_PROMPT: 'cancel_prompt',
  STASH_EVENT: 'stash_event',
  STASH_EVENT_AND_PREVENT_DEFAULT: 'stash_event_and_prevent_default',
  FULLSCREEN_ON_CLICK: 'fullscreen_on_click',
};

const LISTENER = "listener";
const ATTR = "attr";

// These blanks will get filled in when each event comes through.
let gotEventsFrom = ['_'.repeat(LISTENER.length), '_'.repeat(ATTR.length)];

let stashedEvent = null;

function startWorker(worker) {
  navigator.serviceWorker.register(worker);
}

function verifyEvents(eventName) {
  function setTitle() {
    window.document.title = 'Got ' + eventName + ': ' +
      gotEventsFrom.join(', ');
  }

  window.addEventListener(eventName, () => {
    gotEventsFrom[0] = LISTENER;
    setTitle();
  });
  window['on' + eventName] = () => {
    gotEventsFrom[1] = ATTR;
    setTitle();
  };
}

function callPrompt(event) {
  event.prompt();
  event.userChoice.then(function(choiceResult) {
    window.document.title = 'Got userChoice: ' + choiceResult.outcome;
  });
}

function callStashedPrompt() {
  if (stashedEvent === null) {
      throw new Error('No event was previously stashed');
  }
  callPrompt(stashedEvent);
}

function isBodyFullscreen() {
  return document.fullscreenElement == document.body;
}

function toggleFullscreen() {
  if (isBodyFullscreen()) {
    document.exitFullscreen();
  } else {
    document.body.requestFullscreen();
  }
}

function addClickListener(action) {
  switch (action) {
    case Action.CALL_STASHED_PROMPT_ON_CLICK:
      window.addEventListener('click', callStashedPrompt);
      break;
    case Action.CALL_STASHED_PROMPT_ON_CLICK_VERIFY_APPINSTALLED:
      window.addEventListener('click', callStashedPrompt);
      verifyEvents("appinstalled");
      break;
    case Action.FULLSCREEN_ON_CLICK:
      window.addEventListener('click', toggleFullscreen);
      break;
  }
}

function addPromptListener(action) {
  window.addEventListener('beforeinstallprompt', function(e) {
    switch (action) {
      case Action.VERIFY_APPINSTALLED_STASH_EVENT:
        stashedEvent = e;
        verifyEvents('appinstalled');
        break;
      case Action.CALL_PROMPT_DELAYED:
        e.preventDefault();
        setTimeout(callPrompt, 0, e);
        break;
      case Action.CALL_PROMPT_IN_HANDLER:
        callPrompt(e);
        break;
      case Action.CALL_PROMPT_NO_USERCHOICE:
        e.preventDefault();
        setTimeout(() => e.prompt(), 0);
        break;
      case Action.CANCEL_PROMPT:
        e.preventDefault();
        break;
      case Action.CANCEL_PROMPT_AND_NAVIGATE:
        e.preventDefault();
        // Navigate the window to trigger cancellation in the renderer.
        setTimeout(function() { window.location.href = "/" }, 0);
        break;
      case Action.STASH_EVENT:
        stashedEvent = e;
        break;
      case Action.STASH_EVENT_AND_PREVENT_DEFAULT:
        stashedEvent = e;
        e.preventDefault();
        break;
    }
  });
}

function addManifestLinkTag(optionalCustomUrl) {
  const url = new URL(window.location.href);
  let manifestUrl = url.searchParams.get('manifest');
  if (!manifestUrl) {
    manifestUrl = optionalCustomUrl || 'manifest.json';
  }

  var linkTag = document.createElement("link");
  linkTag.id = "manifest";
  linkTag.rel = "manifest";
  linkTag.href = manifestUrl;
  document.head.append(linkTag);
}

function addFavicon(favicon_url) {
  var linkTag = document.createElement("link");
  linkTag.id = "new-icon";
  linkTag.rel = "icon";
  linkTag.type = "image/x-icon";
  linkTag.href = favicon_url;
  document.head.append(linkTag);
}

function removeAllManifestTags() {
  for (let i = 0; i < document.head.children.length; ++i) {
    let child = document.head.children[i];
    if (child.rel == "manifest")
      child.parentElement.removeChild(child);
  }
}

function initialize() {
  const url = new URL(window.location.href);
  initializeActions(url.searchParams.get('action'));
  addOtherTags(url);
}

function initializeActions(action) {
  if (!action) {
    return;
  }

  switch (action) {
    case Action.VERIFY_APPINSTALLED:
      verifyEvents('appinstalled');
      break;
    case Action.VERIFY_PROMPT_APPINSTALLED:
      addPromptListener(Action.CALL_PROMPT_NO_USERCHOICE);
      verifyEvents('appinstalled');
      break;
    case Action.VERIFY_BEFOREINSTALLPROMPT:
      verifyEvents('beforeinstallprompt');
      break;
    case Action.CALL_STASHED_PROMPT_ON_CLICK:
    case Action.CALL_STASHED_PROMPT_ON_CLICK_VERIFY_APPINSTALLED:
      addPromptListener(Action.STASH_EVENT);
      addClickListener(action);
      break;
    case Action.STASH_EVENT_AND_PREVENT_DEFAULT:
      addPromptListener(action);
      addClickListener(Action.CALL_STASHED_PROMPT_ON_CLICK);
      break;
    case Action.VERIFY_APPINSTALLED_STASH_EVENT:
    case Action.CALL_PROMPT_DELAYED:
    case Action.CALL_PROMPT_IN_HANDLER:
    case Action.CALL_PROMPT_NO_USERCHOICE:
    case Action.CANCEL_PROMPT_AND_NAVIGATE:
    case Action.CANCEL_PROMPT:
    case Action.STASH_EVENT:
      addPromptListener(action);
      break;
    case Action.FULLSCREEN_ON_CLICK:
      addClickListener(action);
      break;
    default:
      throw new Error("Unrecognised action: " + action);
  }
}

function initializeWithWorker(worker) {
  startWorker(worker);
  initialize();
}

function changeManifestUrl(newManifestUrl) {
  var linkTag = document.getElementById("manifest");
  linkTag.href = newManifestUrl;
}

function addOtherTags(url) {
  for (const [key, value] of url.searchParams) {
    if (key === "manifest" || key === "action" || !value) {
      continue;
    }
    if (key === 'icon') {
      var linkTag = document.createElement("link");
      linkTag.id = key
      linkTag.rel = key;
      linkTag.href = value;
      document.head.append(linkTag);
    }
    else {
      var meta = document.createElement('meta');
      meta.name = key
      meta.content = value
      document.head.append(meta);
    }
  }
}
