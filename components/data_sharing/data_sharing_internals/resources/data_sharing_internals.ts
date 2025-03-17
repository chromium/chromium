// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {DataSharingInternalsBrowserProxy} from './data_sharing_internals_browser_proxy.js';
import type {GroupData, GroupMember} from './group_data.mojom-webui.js';
import {MemberRole} from './group_data.mojom-webui.js';
import {LogSource} from './logger_common.mojom-webui.js';

function getProxy(): DataSharingInternalsBrowserProxy {
  return DataSharingInternalsBrowserProxy.getInstance();
}

// Contains all the log events received when the internals page is open.
const logMessages: Array<{
  eventTime: string,
  logSource: string,
  sourceLocation: string,
  message: string,
}> = [];

/**
 * Converts a mojo time to JS. TODO(crbug.com/200327630)
 */
function convertMojoTimeToJS(mojoTime: Time): Date {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

/**
 * Appends a new TD element to the specified |parent| element, and returns the
 * newly created element.
 *
 * @param {HTMLTableRowElement} parent The element to which a new TD element is
 *     appended.
 * @param {string} textContent The inner HTML of the element.
 * @param {string} className The class name of the element.
 */
function appendTD(
    parent: HTMLTableRowElement, textContent: string, className: string) {
  const td = parent.insertCell();
  td.textContent = textContent;
  td.className = className;
  parent.appendChild(td);
  return td;
}

/**
 * Maps the logSource to a human readable string representation.
 * Must be kept in sync with the |LogSource| enum in
 * //components/data_sharing/public/logger_common.mojom.
 * @param logSource
 * @returns string
 */
function logSourceToString(logSource: LogSource): string {
  switch (logSource) {
    case LogSource.Unknown:
      return 'Unknown';
    case LogSource.CollaborationService:
      return 'CollaborationService';
    case LogSource.DataSharingService:
      return 'DataSharingService';
    case LogSource.TabGroupSyncService:
      return 'TabGroupSyncService';
    case LogSource.UI:
      return 'UI';
    default:
      return 'N/A';
  }
}


function appendTextChildToList(textToShow: string, element: HTMLUListElement) {
  const textElement = document.createElement('li');
  textElement.textContent = textToShow;
  element.appendChild(textElement);
}

function roleTypeToString(role: MemberRole): string {
  switch (role) {
    case MemberRole.kUnspecified:
      return 'Unknown';
    case MemberRole.kOwner:
      return 'Owner';
    case MemberRole.kMember:
      return 'Member';
    case MemberRole.kInvitee:
      return 'Invitee';
    case MemberRole.kFormerMember:
      return 'FormerMember';
  }
}

function addMemberToGroup(member: GroupMember, group: HTMLUListElement) {
  const memberlistItem = document.createElement('li');
  const memberItem = document.createElement('ul');
  appendTextChildToList(member.displayName, memberItem);
  appendTextChildToList(member.email, memberItem);
  appendTextChildToList(roleTypeToString(member.role), memberItem);
  appendTextChildToList(member.avatarUrl.url, memberItem);
  appendTextChildToList(member.givenName, memberItem);
  memberlistItem.appendChild(memberItem);
  group.appendChild(memberlistItem);
}


/**
 * Show all groups information.
 */
function displayGroups(isSuccess: boolean, groupData: GroupData[]) {
  getRequiredElement('get-all-groups-status').textContent =
      isSuccess ? 'success' : 'failed';
  const groupList = getRequiredElement('group-list');
  groupData.forEach((group) => {
    const listItem = document.createElement('li');
    const groupItem = document.createElement('ul');
    appendTextChildToList(group.groupId, groupItem);
    appendTextChildToList(group.displayName, groupItem);
    group.members.forEach((member) => {
      addMemberToGroup(member, groupItem);
    });
    listItem.appendChild(groupItem);
    groupList.appendChild(listItem);
  });
}

function onLogMessagesDump() {
  const data = JSON.stringify(logMessages);
  const blob = new Blob([data], {'type': 'text/json'});
  const url = URL.createObjectURL(blob);
  const filename = 'data_sharing_internals_logs_dump.json';

  const a = document.createElement('a');
  a.setAttribute('href', url);
  a.setAttribute('download', filename);

  const event = document.createEvent('MouseEvent');
  event.initMouseEvent(
      'click', true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0,
      null);
  a.dispatchEvent(event);
}

function initialize() {
  const tabbox = document.querySelector('cr-tab-box');
  assert(tabbox);
  tabbox.hidden = false;

  const logMessageContainer =
      getRequiredElement<HTMLTableElement>('log-message-container');

  getRequiredElement('log-messages-dump')
      .addEventListener('click', onLogMessagesDump);

  getProxy().getCallbackRouter().onLogMessageAdded.addListener(
      (eventTime: Time, logSource: number, sourceFile: string,
       sourceLine: number, message: string) => {
        const eventTimeStr = convertMojoTimeToJS(eventTime).toISOString();
        const logSourceStr = logSourceToString(logSource);
        logMessages.push({
          eventTime: eventTimeStr,
          logSource: logSourceStr,
          sourceLocation: `${sourceFile}:${sourceLine}`,
          message,
        });
        const logMessage = logMessageContainer.insertRow();
        logMessage.innerHTML =
            window.trustedTypes ? window.trustedTypes.emptyHTML : '';
        appendTD(logMessage, eventTimeStr, 'event-logs-time');
        appendTD(logMessage, logSourceStr, 'event-logs-log-source');
        appendTD(
            logMessage, `${sourceFile}:${sourceLine}`,
            'event-logs-source-location');
        appendTD(logMessage, message, 'event-logs-message');
      });


  const tabpanelNodeList = document.querySelectorAll('div[slot=\'panel\']');
  const tabpanels = Array.prototype.slice.call(tabpanelNodeList, 0);
  const tabpanelIds = tabpanels.map(function(tab) {
    return tab.id;
  });

  tabbox.addEventListener('selected-index-change', e => {
    const tabpanel = tabpanels[(e as CustomEvent).detail];
    const hash = tabpanel.id.match(/(?:^tabpanel-)(.+)/)[1];
    window.location.hash = hash;
  });

  const activateTabByHash = function() {
    let hash = window.location.hash;

    // Remove the first character '#'.
    hash = hash.substring(1);

    const id = 'tabpanel-' + hash;
    const index = tabpanelIds.indexOf(id);
    if (index === -1) {
      return;
    }
    tabbox.setAttribute('selected-index', `${index}`);

    // TODO: Pull updated group information when selecting that panel.
  };

  window.onhashchange = activateTabByHash;
  activateTabByHash();

  getProxy().getAllGroups().then(
      response => displayGroups(response.isSuccess, response.data));
}

document.addEventListener('DOMContentLoaded', initialize);
