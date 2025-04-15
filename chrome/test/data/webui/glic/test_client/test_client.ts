// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './client.js';
import './sections/scroll_to.js';
import './sections/audio_capture.js';
import './sections/get_page_context.js';
import './sections/sizing.js';
import './sections/permissions.js';
import './sections/file.js';
import './sections/action.js';

import type {OpenSettingsOptions} from '/glic/glic_api/glic_api.js';
import {SettingsPageField, WebClientMode} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from '../api_boot.js';

import {client, getBrowser, logMessage} from './client.js';
import {$} from './page_element_types.js';

createGlicHostRegistryOnLoad().then((registry) => {
  logMessage('registering web client');
  const params = new URLSearchParams(window.location.search);
  const delayMs = Number(params.get('delay_ms'));
  if (delayMs) {
    setTimeout(() => registry.registerWebClient(client), delayMs);
  } else {
    registry.registerWebClient(client);
  }
});

$.pageHeader.addEventListener('contextmenu', function(event) {
  event.preventDefault();
});

$.openGlicSettings.addEventListener('click', () => {
  const selectedHighlight = $.openGlicSettingsHighlight.value;
  logMessage(`Opening settings. Will highlight field: ${selectedHighlight}`);
  const options: OpenSettingsOptions = {};
  if (selectedHighlight === 'osHotkey') {
    options.highlightField = SettingsPageField.OS_HOTKEY;
  } else if (selectedHighlight === 'osEntrypointToggle') {
    options.highlightField = SettingsPageField.OS_ENTRYPOINT_TOGGLE;
  }
  getBrowser()!.openGlicSettingsPage!(options);
});

$.syncCookiesBn.addEventListener('click', async () => {
  $.syncCookieStatus!.innerText = 'Requesting';
  try {
    await getBrowser()!.refreshSignInCookies!();
    $.syncCookieStatus!.innerText = `Done!`;
  } catch (e) {
    $.syncCookieStatus!.innerText = `Caught error: ${e}`;
  }
});

$.testLogsBn.addEventListener('click', () => {
  getBrowser()?.getMetrics?.().onUserInputSubmitted?.(WebClientMode.TEXT);
  getBrowser()?.getMetrics?.().onResponseStarted?.();
  getBrowser()?.getMetrics?.().onResponseStopped?.();
  getBrowser()?.getMetrics?.().onResponseRated?.(true);
  getBrowser()?.getMetrics?.().onUserInputSubmitted?.(WebClientMode.AUDIO);
  getBrowser()?.getMetrics?.().onResponseStarted?.();
  getBrowser()?.getMetrics?.().onResponseStopped?.();
  getBrowser()?.getMetrics?.().onResponseRated?.(false);
  getBrowser()?.getMetrics?.().onSessionTerminated?.();
});

$.testClipboardSave.addEventListener('click', () => {
  navigator.clipboard.writeText('This is some junk!');
});

$.getUserProfileInfoBn.addEventListener('click', async () => {
  $.getUserProfileInfoStatus.innerText = 'Requesting';
  try {
    const profile = await getBrowser()!.getUserProfileInfo!();
    $.getUserProfileInfoStatus.innerText = `Done: ${JSON.stringify(profile)}`;
    const icon = await profile.avatarIcon();
    if (icon) {
      $.getUserProfileInfoImg.src = URL.createObjectURL(icon);
    }
  } catch (e) {
    $.getUserProfileInfoStatus.innerText = `Caught error: ${e}`;
  }
});

$.changeProfileBn.addEventListener('click', () => {
  getBrowser()!.showProfilePicker!();
});

// Add listeners to demo elements:
$.newtabbn.addEventListener('click', async () => {
  const url = $.URL.value;
  const openInBackground = $.createTabInBackground.checked;
  const tabData = await getBrowser()!.createTab!(url, {openInBackground});
  logMessage(`createTab done: ${JSON.stringify(tabData)}`);
});

$.reloadpage.addEventListener('click', () => {
  location.reload();
});

$.contextAccessIndicator.addEventListener('click', () => {
  getBrowser()!.setContextAccessIndicator!($.contextAccessIndicator.checked);
});

$.contextAccessIndicatorV2.addEventListener('click', () => {
  getBrowser()!.setContextAccessIndicator!($.contextAccessIndicatorV2.checked);
});

$.closebn.addEventListener('click', () => {
  getBrowser()!.closePanel!();
});
$.attachpanelbn.addEventListener('click', () => {
  getBrowser()!.attachPanel!();
});
$.detachpanelbn.addEventListener('click', () => {
  getBrowser()!.detachPanel!();
});
$.refreshbn.addEventListener('click', () => {
  location.reload();
});
$.navigateWebviewUrl.addEventListener('keyup', ({key}) => {
  if (key === 'Enter') {
    window.location.href = $.navigateWebviewUrl.value;
  }
});

$.audioDuckingOn.addEventListener('click', () => {
  getBrowser()!.setAudioDucking!(true);
});

$.audioDuckingOff.addEventListener('click', () => {
  getBrowser()!.setAudioDucking!(false);
});

// Hang web client for <workTimeMs> amount of time.
function busyWork(workTimeMs: number) {
  if (workTimeMs < 0) {
    return;
  }

  const end = performance.now() + workTimeMs;
  while (performance.now() < end) {
    // mock busy work to test the web client unresponsive handling.
  }
}

$.hang.addEventListener('click', () => {
  const durationString = $.hangDuration.value;
  const duration = parseFloat(durationString);

  // Validate the input
  if (isNaN(duration)) {
    alert('Invalid number entered for duration.');
    return;
  }
  if (duration < 0) {
    alert('Duration cannot be negative.');
    return;
  }

  const durationMs = duration * 1000;
  busyWork(durationMs);
});

window.addEventListener('load', () => {
  $.desktopScreenshot.addEventListener('click', async () => {
    logMessage('Requesting desktop screenshot...');
    try {
      const screenshot = await getBrowser()!.captureScreenshot!();
      if (screenshot) {
        const blob = new Blob([screenshot.data], {type: 'image/jpeg'});
        $.desktopScreenshotImg.src = URL.createObjectURL(blob);
        $.desktopScreenshotErrorReason!.innerText =
            'Desktop screenshot captured.';
      } else {
        $.desktopScreenshotErrorReason!.innerText =
            'Failed to capture desktop screenshot.';
      }
    } catch (error) {
      $.desktopScreenshotErrorReason!.innerText = `Caught error: ${error}`;
    }
  });
  $.setExperiment.addEventListener('click', async () => {
    const trialName = $.trialName.value;
    const groupName = $.groupName.value;
    $.setExperimentStatus!.innerText +=
        `\nSetting experiment: ${trialName} ${groupName}`;
    await getBrowser()!.setSyntheticExperimentState!(trialName, groupName);
    $.setExperimentStatus!.innerText += '\nExperiment State Set.';
  });
});

$.failInitializationCheckbox.addEventListener('click', () => {
  if ($.failInitializationCheckbox.checked) {
    localStorage.setItem('test-init-failure', 'true');
  } else {
    localStorage.removeItem('test-init-failure');
  }
});
