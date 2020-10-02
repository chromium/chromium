// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const help_app = {
  handler: new helpAppUi.mojom.PageHandlerRemote()
};

// Set up a page handler to talk to the browser process.
helpAppUi.mojom.PageHandlerFactory.getRemote().createPageHandler(
    help_app.handler.$.bindNewPipeAndPassReceiver());

// Set up an index remote to talk to Local Search Service.
/** @type {!chromeos.localSearchService.mojom.IndexProxyRemote} */
const indexRemote = chromeos.localSearchService.mojom.IndexProxy.getRemote();

const GUEST_ORIGIN = 'chrome-untrusted://help-app';
const guestFrame =
    /** @type {!HTMLIFrameElement} */ (document.createElement('iframe'));
guestFrame.src = `${GUEST_ORIGIN}${location.pathname}`;
document.body.appendChild(guestFrame);

// Cached result whether Local Search Service is enabled.
/** @type {Promise<boolean>} */
const isLssEnabled =
    help_app.handler.isLssEnabled().then(result => result.enabled);

/**
 * @param {string} s
 * @return {!mojoBase.mojom.String16Spec}
 */
function toString16(s) {
  return /** @type {!mojoBase.mojom.String16Spec} */ (
      {data: Array.from(s, (/** @type {string} */ c) => c.charCodeAt())});
}
const TITLE_ID = 'title';
const BODY_ID = 'body';
const CATEGORY_ID = 'main-category';

/**
 * A pipe through which we can send messages to the guest frame.
 * Use an undefined `target` to find the <iframe> automatically.
 * Do not rethrow errors, since handlers installed here are expected to
 * throw exceptions that are handled on the other side of the pipe (in the guest
 * frame), not on this side.
 */
const guestMessagePipe = new MessagePipe(
    'chrome-untrusted://help-app', /*target=*/ undefined,
    /*rethrowErrors=*/ false);

guestMessagePipe.registerHandler(Message.OPEN_FEEDBACK_DIALOG, () => {
  return help_app.handler.openFeedbackDialog();
});

guestMessagePipe.registerHandler(Message.SHOW_PARENTAL_CONTROLS, () => {
  help_app.handler.showParentalControls();
});

guestMessagePipe.registerHandler(
    Message.ADD_OR_UPDATE_SEARCH_INDEX, async (message) => {
      if (!(await isLssEnabled)) {
        return;
      }
      const data_from_app =
          /** @type {!Array<!helpApp.SearchableItem>} */ (message);
      const data_to_send = data_from_app.map(searchable_item => {
        const contents = [
          {
            id: TITLE_ID,
            content: toString16(searchable_item.title),
            weight: 1.0,
          },
          {
            id: BODY_ID,
            content: toString16(searchable_item.body),
            weight: 0.2,
          },
          {
            id: CATEGORY_ID,
            content: toString16(searchable_item.mainCategoryName),
            weight: 0.1,
          },
        ];
        return {
          id: searchable_item.id,
          contents,
          locale: searchable_item.locale,
        };
      });
      indexRemote.addOrUpdate(data_to_send);
    });

guestMessagePipe.registerHandler(Message.CLEAR_SEARCH_INDEX, async () => {
  if (!(await isLssEnabled)) {
    return;
  }
  // TODO(b/166047521): Clear the index when that method is available.
});

guestMessagePipe.registerHandler(
    Message.FIND_IN_SEARCH_INDEX, async (message) => {
      if (!(await isLssEnabled)) {
        return {results: null};
      }
      const response = await indexRemote.find(
          toString16((/** @type {{query: string}} */ (message)).query),
          /*max_results=*/ 100);
      if (response.status !==
              chromeos.localSearchService.mojom.ResponseStatus.kSuccess ||
          !response.results) {
        return {results: null};
      }
      const search_results =
          /** @type {!Array<!chromeos.localSearchService.mojom.Result>} */ (
              response.results);
      // Sort results by decreasing score.
      search_results.sort((a, b) => b.score - a.score);
      /** @type {!Array<!helpApp.SearchResult>} */
      const results = search_results.map(result => {
        /** @type {!Array<!helpApp.Position>} */
        const titlePositions = [];
        /** @type {!Array<!helpApp.Position>} */
        const bodyPositions = [];
        for (const position of result.positions) {
          if (position.contentId === TITLE_ID) {
            titlePositions.push(
                {length: position.length, start: position.start});
          } else if (position.contentId === BODY_ID) {
            bodyPositions.push(
                {length: position.length, start: position.start});
          }
        }
        // Sort positions by start index.
        titlePositions.sort((a, b) => a.start - b.start);
        bodyPositions.sort((a, b) => a.start - b.start);
        return {
          id: result.id,
          titlePositions,
          bodyPositions,
        };
      });
      return {results};
    });
