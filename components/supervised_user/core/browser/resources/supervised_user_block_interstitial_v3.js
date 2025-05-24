// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let localWebApprovalsEnabled = false;
let hasSecondCustodian = false;

/** Return the element with the given id. */
function $(id) {
  return document.body.querySelector(`#${id}`);
}

/**
 * Send one of the supported commands to the supervised user error page
 * controller.
 * @param {string} cmd See implementation below
 */
function sendCommand(cmd) {
  if (window.supervisedUserErrorPageController) {
    switch (cmd) {
      case 'back':
        supervisedUserErrorPageController.goBack();
        break;
      case 'requestUrlAccessRemote':
        supervisedUserErrorPageController.requestUrlAccessRemote();
        break;
      case 'requestUrlAccessLocal':
        supervisedUserErrorPageController.requestUrlAccessLocal();
        break;
    }
    return;
  }
  // <if expr="is_ios">
  // Send commands for iOS committed interstitials.
  /** @suppress {undefinedVars|missingProperties} */ (function() {
    window.webkit.messageHandlers['SupervisedUserInterstitialMessage']
        .postMessage({'command': cmd.toString()});
  })();
  // </if>
}

function makeImageSet(url1x, url2x) {
  return 'image-set(url(' + url1x + ') 1x, url(' + url2x + ') 2x)';
}

/** Perform all initialization that can be done at DOMContentLoaded time. */
function initialize() {
  const allowAccessRequests = loadTimeData.getBoolean('allowAccessRequests');
  const custodianName = loadTimeData.getString('custodianName');
  localWebApprovalsEnabled =
      loadTimeData.getBoolean('isLocalWebApprovalsEnabled');

  if (custodianName && allowAccessRequests) {
    const avatarURL1x = loadTimeData.getString('avatarURL1x');
    const avatarURL2x = loadTimeData.getString('avatarURL2x');
    if (avatarURL1x) {
      $('custodian-avatar-img').style.content =
          makeImageSet(avatarURL1x, avatarURL2x);
    }
    $('custodian-name').textContent = custodianName;
    $('custodian-email').textContent = loadTimeData.getString('custodianEmail');
    const secondCustodianName = loadTimeData.getString('secondCustodianName');
    if (secondCustodianName) {
      hasSecondCustodian = true;
      const secondAvatarURL1x = loadTimeData.getString('secondAvatarURL1x');
      const secondAvatarURL2x = loadTimeData.getString('secondAvatarURL2x');
      if (secondAvatarURL1x) {
        $('second-custodian-avatar-img').style.content =
            makeImageSet(secondAvatarURL1x, secondAvatarURL2x);
      }
      $('second-custodian-name').textContent = secondCustodianName;
      $('second-custodian-email').textContent =
          loadTimeData.getString('secondCustodianEmail');
    }
  }

  const alreadyRequestedAccessRemote =
      loadTimeData.getBoolean('alreadySentRemoteRequest');
  if (alreadyRequestedAccessRemote) {
    const isMainFrame = loadTimeData.getBoolean('isMainFrame');
    // Generates the `waiting for permission` page. Safe to exit here
    // early and skip the rest of the IU setup for approval manipulations.
    requestCreated(true, isMainFrame);
    return;
  }

  // The rest of the method sets up the functionality for
  // approval manipulations.
  if (allowAccessRequests) {
    $('remote-approvals-button').hidden = false;
    if (localWebApprovalsEnabled) {
      $('local-approvals-button').hidden = false;
      $('local-approvals-button').classList.add('primary-button');
      $('remote-approvals-button').classList.add('secondary-button');
    }
    $('remote-approvals-button').onclick = function(event) {
      $('remote-approvals-button').disabled = true;
      sendCommand('requestUrlAccessRemote');
    };
    $('local-approvals-button').onclick = function(event) {
      sendCommand('requestUrlAccessLocal');
    };
  } else {
    $('remote-approvals-button').hidden = true;
  }

  // Focus the top-level div for screen readers.
  $('frame-blocked').focus();
}

/**
 * Updates the interstitial to show that the request failed or was sent.
 * @param {boolean} isSuccessful Whether the request was successful or not.
 * @param {boolean} isMainFrame Whether the interstitial is being shown in main
 *     frame.
 */
function setRequestStatus(isSuccessful, isMainFrame) {
  requestCreated(isSuccessful, isMainFrame);
}

/**
 * Updates the interstitial to show that the request failed or was sent.
 * @param {boolean} isSuccessful Whether the request was successful or not.
 * @param {boolean} isMainFrame Whether the interstitial is being shown in main
 *     frame.
 */
function requestCreated(isSuccessful, isMainFrame) {
  $('block-page-header').hidden = true;
  $('block-page-message').hidden = true;
  if (localWebApprovalsEnabled) {
    $('local-approvals-button').hidden = false;
  }
  if (isSuccessful) {
    // Show custodian information.
    $('custodians-information').hidden = false;
    if (hasSecondCustodian) {
      $('second-custodian-information').hidden = false;
      $('second-custodian-avatar-img').hidden = false;
    }
    $('request-failed-message').hidden = true;
    $('request-sent-message').hidden = false;
    $('remote-approvals-button').hidden = true;
    if (localWebApprovalsEnabled) {
      $('local-approvals-button').hidden = true;
      $('local-approvals-remote-request-sent-button').hidden = false;
      $('local-approvals-remote-request-sent-button').onclick = function(
          event) {
        sendCommand('requestUrlAccessLocal');
      };
    } else {
      $('back-button').hidden = !isMainFrame;
      $('back-button').onclick = function(event) {
        sendCommand('back');
      };
    }
    $('error-page-illustration').hidden = true;
    $('waiting-for-approval-illustration').hidden = false;
    $('request-sent-description').hidden = false;
    $('local-approvals-button').classList.add('secondary-button');
  } else {
    $('request-failed-message').hidden = false;
    $('remote-approvals-button').disabled = false;
  }
  // After updating the contents, focus the top-level div for screen readers.
  $('frame-blocked').focus();
}

document.addEventListener('DOMContentLoaded', initialize);
