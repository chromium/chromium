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

  request.formFillingRequests.forEach((formFillingRequest, formIndex) => {
    const li_form = document.createElement('li');
    li_form.textContent =
        `Form for requestedData: ${formFillingRequest.requestedData}`;

    const notifyPresentedBtn = document.createElement('button');
    notifyPresentedBtn.textContent = 'Notify Form Presented';
    notifyPresentedBtn.addEventListener('click', () => {
      request.onFormPresented?.({formFillingRequestIndex: formIndex});
      logMessage(
          `Notified form presented for formFillingRequestIndex ${formIndex}`);
    });
    li_form.appendChild(notifyPresentedBtn);

    const ul_suggestions = document.createElement('ul');
    for (const suggestion of formFillingRequest.suggestions) {
      const li_suggestion = document.createElement('li');
      li_suggestion.textContent = `ID: ${suggestion.id}, Title: ${
          suggestion.title}, Details: ${suggestion.details}`;

      const previewStartBtn = document.createElement('button');
      previewStartBtn.textContent = 'Preview Start';
      previewStartBtn.addEventListener('click', () => {
        request.onFormPreviewChanged?.({
          formFillingRequestIndex: formIndex,
          response: {selectedSuggestionId: suggestion.id},
        });
        logMessage(`Preview started for formFillingRequestIndex ${
            formIndex}, id ${suggestion.id}`);
      });
      li_suggestion.appendChild(previewStartBtn);

      const previewEndBtn = document.createElement('button');
      previewEndBtn.textContent = 'Preview End';
      previewEndBtn.addEventListener('click', () => {
        request.onFormPreviewChanged?.({formFillingRequestIndex: formIndex});
        logMessage(`Preview ended for formFillingRequestIndex ${formIndex}`);
      });
      li_suggestion.appendChild(previewEndBtn);

      ul_suggestions.appendChild(li_suggestion);
    }
    li_form.appendChild(ul_suggestions);

    const confirmBtn = document.createElement('button');
    confirmBtn.textContent = 'Confirm Form';
    confirmBtn.addEventListener('click', () => {
      // Use the first suggestion as the default "confirmation" for this form.
      if (formFillingRequest.suggestions.length > 0) {
        const id = formFillingRequest.suggestions[0]!.id;
        request.onFormConfirmed?.({
          formFillingRequestIndex: formIndex,
          response: {selectedSuggestionId: id},
        });
        logMessage(`Form confirmed for formFillingRequestIndex ${
            formIndex}, id ${id}`);
      }
    });
    li_form.appendChild(confirmBtn);

    $.autofillSuggestionsList.appendChild(li_form);
  });

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
