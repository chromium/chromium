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
/** @type {!chromeos.localSearchService.mojom.IndexRemote} */
const indexRemote =
    chromeos.localSearchService.mojom.Index.getRemote();

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
const SUBHEADING_ID = 'subheading';

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
        /** @type {!Array<!chromeos.localSearchService.mojom.Content>} */
        const contents = [
          {
            id: TITLE_ID,
            content: toString16(searchable_item.title),
            weight: 1.0,
          },
          {
            id: CATEGORY_ID,
            content: toString16(searchable_item.mainCategoryName),
            weight: 0.1,
          },
        ];
        // If there are subheadings, use those instead of the body.
        if (searchable_item.subheadings
            && searchable_item.subheadings.length > 0) {
          for (let i = 0; i < searchable_item.subheadings.length; ++i) {
            contents.push({
              id: SUBHEADING_ID + i,
              content: toString16(searchable_item.subheadings[i]),
              weight: 0.4,
            });
          }
        } else if (searchable_item.body) {
          contents.push({
            id: BODY_ID,
            content: toString16(searchable_item.body),
            weight: 0.2,
          });
        }
        return {
          id: searchable_item.id,
          contents,
          locale: searchable_item.locale,
        };
      });
      return indexRemote.addOrUpdate(data_to_send);
    });

guestMessagePipe.registerHandler(Message.CLEAR_SEARCH_INDEX, async () => {
  if (!(await isLssEnabled)) {
    return;
  }
  return indexRemote.clearIndex();
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
        // Id of the best subheading that appears in positions. We consider
        // the subheading containing the most match positions to be the best.
        // "" means no subheading positions found.
        let bestSubheadingId = "";
        /**
         * Counts how many positions there are for each subheading id.
         * @type {!Object<string, number>}
         */
        const subheadingPosCounts = {};
        // Note: result.positions is not sorted.
        for (const position of result.positions) {
          if (position.contentId === TITLE_ID) {
            titlePositions.push(
                {length: position.length, start: position.start});
          } else if (position.contentId === BODY_ID) {
            bodyPositions.push(
                {length: position.length, start: position.start});
          } else if (position.contentId.startsWith(SUBHEADING_ID)) {
            // Update the subheadings's position count and check if it's the new
            // best subheading.
            const newCount = (subheadingPosCounts[position.contentId] || 0) + 1;
            subheadingPosCounts[position.contentId] = newCount;
            if (!bestSubheadingId
                || newCount > subheadingPosCounts[bestSubheadingId]) {
              bestSubheadingId = position.contentId;
            }
          }
        }
        // Use only the positions of the best subheading.
        /** @type {!Array<!helpApp.Position>} */
        const subheadingPositions = [];
        if (bestSubheadingId) {
          for (const position of result.positions) {
            if (position.contentId === bestSubheadingId) {
              subheadingPositions.push({
                start: position.start,
                length: position.length,
              });
            }
          }
          subheadingPositions.sort(compareByStart);
        }

        // Sort positions by start index.
        titlePositions.sort(compareByStart);
        bodyPositions.sort(compareByStart);
        return {
          id: result.id,
          titlePositions,
          bodyPositions,
          subheadingIndex: bestSubheadingId
              ? Number(bestSubheadingId.substring(SUBHEADING_ID.length))
              : null,
          subheadingPositions: bestSubheadingId
              ? subheadingPositions
              : null,
        };
      });
      return {results};
    });

/**
 * Compare two positions by their start index. Use for sorting.
 *
 * @param {!helpApp.Position} a
 * @param {!helpApp.Position} b
 */
function compareByStart(a, b) {
  return a.start - b.start;
}
