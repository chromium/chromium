// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SelectCredentialDialogRequest, Subscriber} from '/glic/glic_api/glic_api.js';
import {CredentialType, UserGrantedPermissionDuration} from '/glic/glic_api/glic_api.js';

import {client, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

function credentialTypeAsString(credType: CredentialType): string {
  switch (credType) {
    case CredentialType.PASSWORD:
      return 'Password';
    case CredentialType.FEDERATED:
      return 'Federated';
    default:
      return 'Unknown type';
  }
}

function setIcon(imgElem: HTMLImageElement, getter?: () => Promise<Blob>) {
  URL.revokeObjectURL(imgElem.src);
  if (getter) {
    getter().then((imageBlob) => {
      imgElem.src = URL.createObjectURL(imageBlob);
    });
  } else {
    imgElem.src = '';
  }
}

function convertUint8ArrayToBase64(uint8Array: Uint8Array): string {
  let binaryString = '';
  for (let i = 0; i < uint8Array.length; i++) {
    binaryString += String.fromCharCode(uint8Array[i]!);
  }
  return btoa(binaryString);
}

$.executeAction.addEventListener('click', async () => {
  logMessage('Starting Execute Action');

  // The action proto is expected to be a Actions proto, which is binary
  // serialized and then base64 encoded.
  const protoBytes = Uint8Array.fromBase64($.actionProtoEncodedText.value);
  try {
    const actionResult = await client!.browser!.performActions!
                         (protoBytes.buffer as ArrayBuffer);
    $.actionStatus.innerText = `Finished Execute Action. Result code ${
        convertUint8ArrayToBase64(new Uint8Array(actionResult))}.`;
  } catch (error) {
    $.actionStatus.innerText = `Error in Execute Action: ${error}`;
  }
});

let credentialHandlerSubscriber: Subscriber|undefined;

function initCredentialHandlingIfNeeded() {
  if (credentialHandlerSubscriber) {
    return;
  }

  credentialHandlerSubscriber =
      client.browser?.selectCredentialDialogRequestHandler?.().subscribe(
          showCredentialPicker);
}

$.createActorTask.addEventListener('click', async () => {
  logMessage('Starting Create Actor Task');
  try {
    const taskId = await client!.browser!.createTask!();
    $.actorTaskId.value = taskId.toString();
    $.actionStatus.innerText = `Created task with ID: ${taskId}`;
  } catch (error) {
    $.actionStatus.innerText = `Error in Create Actor Task: ${error}`;
  }

  // We only handle credentials when running interactively, which is why we set
  // up the handling here. If we did this on load, then we'd interfere with
  // automated tests which need to do their own custom handling.
  initCredentialHandlingIfNeeded();
});

$.stopActorTask.addEventListener('click', () => {
  logMessage('Starting Stop Actor Task');
  const taskIdStr = $.actorTaskId.value;
  if (taskIdStr) {
    const taskId = parseInt(taskIdStr, 10);
    if (isNaN(taskId)) {
      $.actionStatus.innerText = `Invalid task ID: ${taskIdStr}`;
      return;
    }
    client!.browser!.stopActorTask!(taskId);
    $.actionStatus.innerText = `Stopped task with ID: ${taskId}`;
  } else {
    client!.browser!.stopActorTask!();
    $.actionStatus.innerText = 'Stopped most recent task.';
  }
});

let lastCredentialRequest: SelectCredentialDialogRequest|null = null;

function pickCredential(once: boolean) {
  if (!lastCredentialRequest) {
    return;
  }

  client.browser?.uninterruptActorTask?.(lastCredentialRequest.taskId);

  const select = $.selectCredential;

  lastCredentialRequest.onDialogClosed({
    response: {
      taskId: lastCredentialRequest.taskId,
      selectedCredentialId: parseInt(select.value),
      permissionDuration: once ? UserGrantedPermissionDuration.ONE_TIME :
                                 UserGrantedPermissionDuration.ALWAYS_ALLOW,
    },
  });

  lastCredentialRequest = null;
  $.credentialSelection.style.display = 'none';
}

function updateSelectedCredentialDetails() {
  if (!lastCredentialRequest) {
    return;
  }

  const select = $.selectCredential;
  const selectedCredential = lastCredentialRequest.credentials.find((cred) => {
    return `${cred.id}` === select.value;
  });
  if (!selectedCredential) {
    return;
  }

  $.selectedCredentialUsername.innerText = selectedCredential.username;
  $.selectedCredentialSource.innerText = selectedCredential.sourceSiteOrApp;
  $.selectedCredentialRequestOrigin.innerText =
      selectedCredential.requestOrigin!;
  setIcon($.selectedCredentialIcon, selectedCredential.getIcon);
  $.selectedCredentialType.innerText =
      credentialTypeAsString(selectedCredential.type!);
  setIcon(
      $.selectedCredentialAccountPicture, selectedCredential.getAccountPicture);
}

function showCredentialPicker(request: SelectCredentialDialogRequest) {
  const select = $.selectCredential;
  select.innerHTML = '';
  request.credentials.forEach((cred) => {
    const opt = document.createElement('option');
    opt.value = `${cred.id}`;
    opt.text = cred.username;
    select.add(opt);
  });

  lastCredentialRequest = request;

  updateSelectedCredentialDetails();

  $.credentialSelection.style.display = 'block';
  client.browser?.interruptActorTask?.(request.taskId);
}

$.selectCredential.addEventListener('change', () => {
  updateSelectedCredentialDetails();
});

$.credentialOnce.addEventListener('click', () => {
  pickCredential(true);
});

$.credentialAlways.addEventListener('click', () => {
  pickCredential(false);
});
