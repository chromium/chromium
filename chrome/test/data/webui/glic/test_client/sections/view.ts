// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewChangeRequest} from '/glic/glic_api/glic_api.js';
import {ClientView} from '/glic/glic_api/glic_api.js';

import {client, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

window.addEventListener('load', async () => {
  for (const [key, value] of Object.entries(ClientView)) {
    const option = document.createElement('option');
    option.textContent = `${key}: ${value}`;
    option.value = value;
    $.viewChangedCurrentView.appendChild(option);
  }
  const invalidOption = document.createElement('option');
  invalidOption.textContent = 'invalid';
  invalidOption.value = 'invalid';
  $.viewChangedCurrentView.appendChild(invalidOption);

  $.viewChangedBtn.addEventListener('click', () => {
    const currentView = $.viewChangedCurrentView.value as ClientView;
    client.browser!.onViewChanged!({currentView});
  });

  await client.getInitialized();

  if (!client.browser?.onViewChanged) {
    $.viewChangedBtn.disabled = true;
    $.viewChangedAutomaticallyAccept.checked = false;
    $.viewChangedAutomaticallyAccept.disabled = true;
  }

  client.browser?.getViewChangeRequests?.()?.subscribe(
      (request: ViewChangeRequest) => {
        logMessage(`requestViewChange(${JSON.stringify(request)})`);
        if ($.viewChangedAutomaticallyAccept.checked) {
          $.viewChangedCurrentView.value = request.desiredView;
          client.browser!.onViewChanged!({currentView: request.desiredView});
        }
      });
});
