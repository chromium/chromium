// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Provides basic utility functions and variables for
 * media-router-container tests.
 */

cr.define('media_router_container_test_base', function() {
  function init(container) {
    /**
     * Checks whether |view| matches the current view of |container|.
     *
     * @param {!media_router.MediaRouterView} view Expected view type.
     */
    var checkCurrentView = function(view) {
      assertEquals(view, container.currentView_);
    };

    /**
     * Checks whether the elements specified in |elementIdList| are visible.
     * Checks whether all other elements are not visible. Throws an assertion
     * error if this is not true.
     *
     * @param {!Array<!string>} elementIdList List of id's of elements that
     *     should be visible.
     */
    var checkElementsVisibleWithId = function(elementIdList) {
      for (var id of elementIdList)
        checkElementVisibleWithId(true, id);

      for (id of hiddenCheckElementIdList) {
        if (!elementIdList.includes(id)) {
          if (id === 'first-run-flow-cloud-pref' &&
              !elementIdList.includes('first-run-flow')) {
            // If 'first-run-flow' is already expected to be hidden, don't check
            // first-run-flow-cloud-pref which is a child of it. Polymer2
            // optimizes <dom-if>s that are false, by no longer updating its
            // contents.
            continue;
          }
          if ((id === 'search-results' || id === 'no-search-matches') &&
              !elementIdList.includes('search-results-container')) {
            // If 'search-results-container' is already expected to be hidden,
            // don't check search-results or no-search-matches which are
            // children of it. Polymer2 optimizes <dom-if>s that are false, by
            // no longer updating its contents.
            continue;
          }

          checkElementVisibleWithId(false, id);
        }
      }
    };

    /**
     * Checks the visibility of an element. An element is considered visible if
     * it exists and its |hidden| property is |false|.
     *
     * @param {boolean} visible Whether the element should be visible.
     * @param {?Element} element The element to test.
     * @param {?string} elementId Optional element id to display.
     */
    var checkElementVisible = function(visible, element, elementId) {
      var elementVisible =
          !!element && !element.hidden && element.style.display != 'none';
      assertEquals(visible, elementVisible, elementId);
    };

    /**
     * Checks the visibility of an element with |elementId| in |container|.
     * An element is considered visible if it exists and its |hidden| property
     * is |false|.
     *
     * @param {boolean} visible Whether the element should be visible.
     * @param {!string} elementId The id of the element to test.
     */
    var checkElementVisibleWithId = function(visible, elementId) {
      var element = container.$$('#' + elementId);
      checkElementVisible(visible, element);
    };

    /**
     * Checks whether |expected| and the text in the |element| are equal.
     *
     * @param {!string} expected Expected text.
     * @param {!Element} element Element whose text will be checked.
     */
    var checkElementText = function(expected, element) {
      assertEquals(expected.trim(), element.textContent.trim());
    };

    /**
     * The blocking issue to show.
     * @type {!media_router.Issue}
     */
    var fakeBlockingIssue = new media_router.Issue(
        1, 'Issue Title 1', 'Issue Message 1', 0, 1, 'route id 1', true, 1234);

    /**
     * The list of CastModes to show.
     * @type {!Array<!media_router.CastMode>}
     */
    var fakeCastModeList = [
      new media_router.CastMode(
          media_router.CastModeType.PRESENTATION, 'Cast google.com',
          'google.com', false),
      new media_router.CastMode(
          media_router.CastModeType.TAB_MIRROR, 'Description 1', null, false),
      new media_router.CastMode(
          media_router.CastModeType.DESKTOP_MIRROR, 'Description 2', null,
          false),
    ];

    /**
     * The list of CastModes to show with non-PRESENTATION modes only.
     * @type {!Array<!media_router.CastMode>}
     */
    var fakeCastModeListWithNonPresentationModesOnly = [
      new media_router.CastMode(
          media_router.CastModeType.TAB_MIRROR, 'Description 1', null, false),
      new media_router.CastMode(
          media_router.CastModeType.DESKTOP_MIRROR, 'Description 2', null,
          false),
    ];

    /**
     * The list of CastModes to show with PRESENTATION forced.
     * @type {!Array<!media_router.CastMode>}
     */
    var fakeCastModeListWithPresentationModeForced = [
      new media_router.CastMode(
          media_router.CastModeType.PRESENTATION, 'Cast google.com',
          'google.com', true),
      new media_router.CastMode(
          media_router.CastModeType.DESKTOP_MIRROR, 'Description 2', null,
          false),
      new media_router.CastMode(
          media_router.CastModeType.LOCAL_FILE, 'Description 3', null, false),
    ];

    /**
     * The list of CastModes to show with Local media on the list
     * @type {!Array<!media_router.CastMode>}
     */
    var fakeCastModeListWithLocalMedia = [
      new media_router.CastMode(
          media_router.CastModeType.TAB_MIRROR, 'Description 1', null, false),
      new media_router.CastMode(
          media_router.CastModeType.DESKTOP_MIRROR, 'Description 2', null,
          false),
      new media_router.CastMode(
          media_router.CastModeType.LOCAL_FILE, 'Description 3', null, false),
    ];

    /**
     * The blocking issue to show.
     * @type {!media_router.Issue}
     */
    var fakeNonBlockingIssue = new media_router.Issue(
        2, 'Issue Title 2', 'Issue Message 2', 0, 1, 'route id 2', false, 1234);

    /**
     * The list of current routes.
     * @type {!Array<!media_router.Route>}
     */
    var fakeRouteList = [
      new media_router.Route('id 1', 'sink id 1', 'Title 1', 0, true, false),
      new media_router.Route('id 2', 'sink id 2', 'Title 2', 1, false, true),
    ];

    /**
     * The list of current routes with local routes only.
     * @type {!Array<!media_router.Route>}
     */
    var fakeRouteListWithLocalRoutesOnly = [
      new media_router.Route('id 1', 'sink id 1', 'Title 1', 0, true, false),
      new media_router.Route('id 2', 'sink id 2', 'Title 2', 1, true, false),
    ];

    // Common cast mode bitset for creating sinks in |fakeSinkList|.
    var castModeBitset = 0x2 | 0x4 | 0x8;
    /**
     * The list of available sinks.
     * @type {!Array<!media_router.Sink>}
     */
    var fakeSinkList = [
      new media_router.Sink(
          'sink id 1', 'Sink 1', null, null, media_router.SinkIconType.CAST,
          media_router.SinkStatus.ACTIVE, castModeBitset),
      new media_router.Sink(
          'sink id 2', 'Sink 2', null, null, media_router.SinkIconType.CAST,
          media_router.SinkStatus.ACTIVE, castModeBitset),
      new media_router.Sink(
          'sink id 3', 'Sink 3', null, null, media_router.SinkIconType.CAST,
          media_router.SinkStatus.PENDING, castModeBitset),
    ];

    /**
     * The list of elements to check for visibility.
     * @const {!Array<!string>}
     */
    var hiddenCheckElementIdList = [
      'cast-mode-list',
      'container-header',
      'device-missing',
      'first-run-flow',
      'first-run-flow-cloud-pref',
      'issue-banner',
      'no-search-matches',
      'route-details',
      'search-results',
      'search-results-container',
      'sink-list',
      'sink-list-view',
    ];

    /**
     * Search text that will match all sinks.
     * @type {!string}
     */
    var searchTextAll = 'sink';

    /**
     * Search text that won't match any sink in fakeSinkList.
     * @type {!string}
     */
    var searchTextNone = 'abc';

    /**
     * Search text that will match exactly one sink.
     * @type {!string}
     */
    var searchTextOne = 'sink 1';

    return {
      checkCurrentView: checkCurrentView,
      checkElementsVisibleWithId: checkElementsVisibleWithId,
      checkElementVisible: checkElementVisible,
      checkElementVisibleWithId: checkElementVisibleWithId,
      checkElementText: checkElementText,
      fakeBlockingIssue: fakeBlockingIssue,
      fakeCastModeList: fakeCastModeList,
      fakeCastModeListWithNonPresentationModesOnly:
          fakeCastModeListWithNonPresentationModesOnly,
      fakeCastModeListWithPresentationModeForced:
          fakeCastModeListWithPresentationModeForced,
      fakeCastModeListWithLocalMedia: fakeCastModeListWithLocalMedia,
      fakeNonBlockingIssue: fakeNonBlockingIssue,
      fakeRouteList: fakeRouteList,
      fakeRouteListWithLocalRoutesOnly: fakeRouteListWithLocalRoutesOnly,
      fakeSinkList: fakeSinkList,
      searchTextAll: searchTextAll,
      searchTextNone: searchTextNone,
      searchTextOne: searchTextOne,
    };
  }
  return {init: init};
});
