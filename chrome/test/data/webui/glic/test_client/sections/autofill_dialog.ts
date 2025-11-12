// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SelectAutofillSuggestionsDialogRequest} from '/glic/glic_api/glic_api.js';

import {client, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

let currentRequest: SelectAutofillSuggestionsDialogRequest|null = null;

function clearUi() {
  $.autofillSuggestionsDialogStatus.textContent = 'Idle';
  $.autofillSuggestionsList.innerHTML = '';
  $.selectedAutofillSuggestionId.value = '';
  $.autofillSuggestionsDialogSection.hidden = true;
}

function handleRequest(request: SelectAutofillSuggestionsDialogRequest) {
  currentRequest = request;
  $.autofillSuggestionsDialogSection.hidden = false;
  $.autofillSuggestionsDialogStatus.textContent =
      'Task requests suggestion selection.';

  for (const formFillingRequest of request.formFillingRequests) {
    const li_form = document.createElement('li');
    li_form.textContent =
        `Form for requestedData: ${formFillingRequest.requestedData}`;
    const ul_suggestions = document.createElement('ul');
    for (const suggestion of formFillingRequest.suggestions) {
      const li_suggestion = document.createElement('li');
      li_suggestion.textContent = `ID: ${suggestion.id}, Title: ${
          suggestion.title}, Details: ${suggestion.details}`;
      ul_suggestions.appendChild(li_suggestion);
    }
    li_form.appendChild(ul_suggestions);
    $.autofillSuggestionsList.appendChild(li_form);
  }

  if (request.formFillingRequests.length > 0) {
    // Pre-fill the selection with the first suggestion from each form filling
    // request.
    $.selectedAutofillSuggestionId.value =
        String(request.formFillingRequests
                   .map((ffReq) => ffReq.suggestions[0]?.id || '')
                   .reduce((a, b) => a + (a === '' ? '' : ',') + b, ''));
  }
}

$.sendAutofillSuggestionsResponse.addEventListener('click', () => {
  if (!currentRequest) {
    return;
  }
  const selectedIds = ($.selectedAutofillSuggestionId.value || '').split(',');
  currentRequest.onDialogClosed({
    response: {
      selectedSuggestions: selectedIds.map((id) => {
        return {
          selectedSuggestionId: id,
        };
      }),
    },
  });
  logMessage('Sent autofill suggestion response.');
  clearUi();
  currentRequest = null;
});

$.cancelAutofillSuggestionsDialog.addEventListener('click', () => {
  if (!currentRequest) {
    return;
  }
  currentRequest.onDialogClosed({
    response: {
      selectedSuggestions: [],
    },
  });
  logMessage('Sent empty selection suggestion response.');
  clearUi();
  currentRequest = null;
});


client.getInitialized().then(() => {
  const handler =
      client.browser?.selectAutofillSuggestionsDialogRequestHandler?.();
  if (handler) {
    handler.subscribe(handleRequest);
    clearUi();
  } else {
    $.autofillSuggestionsDialogSection.hidden = true;
    logMessage('Autofill suggestion dialog handler not available.');
  }
});
