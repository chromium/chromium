// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for media-router-container specifically related
 * to the filter view.
 */
cr.define('media_router_container_filter', function() {
  /**
   * Wrapper that lets a function |f| run after the container animation promise
   * completes but also lets any UI logic run before setting up the call. This
   * is important because |container.animationPromise_| may not exist until the
   * UI logic runs or it may be updated to a new Promise.  This wrapper also
   * carries assertion errors (and any other exceptions) outside of the promise
   * back into the test since throwing in a then() or catch() doesn't stop the
   * test.
   *
   * @param {function()} f
   */
  var chainOnAnimationPromise = function(f) {
    setTimeout(function() {
      container.animationPromise_.then(f).catch(function(err) {
        setTimeout(function() {
          throw err;
        });
      });
    });
  };

  /**
   * Checks whether |view| matches the current view of |container|.
   *
   * @param {!media_router.MediaRouterView} view Expected view type.
   */
  var checkCurrentView;

  /**
   * Checks whether the elements specified in |elementIdList| are visible.
   * Checks whether all other elements are not visible. Throws an assertion
   * error if this is not true.
   *
   * @param {!Array<!string>} elementIdList List of id's of elements that
   *     should be visible.
   */
  var checkElementsVisibleWithId;

  /**
   * Checks whether |expected| and the text in the |element| are equal.
   *
   * @param {!string} expected Expected text.
   * @param {!Element} element Element whose text will be checked.
   */
  var checkElementText;

  /**
   * Media Router Container created before each test.
   * @type {?MediaRouterContainer}
   */
  var container;

  /**
   * The blocking issue to show.
   * @type {?media_router.Issue}
   */
  var fakeNonBlockingIssue;

  /**
   * The list of current routes.
   * @type {!Array<!media_router.Route>}
   */
  var fakeRouteList = [];

  /**
   * The list of available sinks.
   * @type {!Array<!media_router.Sink>}
   */
  var fakeSinkList = [];

  /**
   * Simulates pressing the Escape key on |element|.
   * @param {!HTMLElement} element
   */
  var pressEscapeOnElement = function(element) {
    element.dispatchEvent(new KeyboardEvent('keydown', {
      'key': 'Escape',
      'code': 'Escape',
      'bubbles': true,
      'composed': true,
      'cancelable': true
    }));
  };

  /**
   * Search text that will match all sinks.
   * @type {?string}
   */
  var searchTextAll;

  /**
   * Search text that won't match any sink in fakeSinkList.
   * @type {?string}
   */
  var searchTextNone;

  /**
   * Search text that will match exactly one sink.
   * @type {?string}
   */
  var searchTextOne;

  /**
   * Import media_router_container.html before running suite.
   */
  var doSuiteSetup = function() {
    return PolymerTest.importHtml(
        'chrome://media-router/elements/media_router_container/' +
        'media_router_container.html');
  };

  /**
   * Performs test setup before each test.
   *
   * @param {function()} done Function for async test completion.
   */
  var doSetup = function(done) {
    PolymerTest.clearBody();
    // Initialize a media-router-container before each test.
    container = document.createElement('media-router-container');
    document.body.appendChild(container);

    // Get common functions and variables.
    var test_base = media_router_container_test_base.init(container);

    checkCurrentView = test_base.checkCurrentView;
    checkElementsVisibleWithId = test_base.checkElementsVisibleWithId;
    checkElementText = test_base.checkElementText;
    fakeNonBlockingIssue = test_base.fakeNonBlockingIssue;
    fakeRouteList = test_base.fakeRouteList;
    fakeSinkList = test_base.fakeSinkList;
    searchTextAll = test_base.searchTextAll;
    searchTextNone = test_base.searchTextNone;
    searchTextOne = test_base.searchTextOne;

    container.castModeList = test_base.fakeCastModeList;
    container.searchEnabled_ = true;

    // Allow for the media router container to be created, attached, and
    // listeners registered in an afterNextRender() call.
    Polymer.RenderStatus.afterNextRender(this, done);
  };

  function registerTestsPart1() {
    suite('MediaRouterContainerFilterPart1', function() {
      suiteSetup(doSuiteSetup);
      setup(doSetup);

      // Tests that clicking the search icon will cause the container to enter
      // filter view.
      test('click search icon', function(done) {
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          checkCurrentView(media_router.MediaRouterView.FILTER);
          assertEquals(
              container.$$('#sink-search-input'),
              container.shadowRoot.activeElement);
          done();
        });
      });

      // Tests that focusing the sink search input will cause the container to
      // enter filter view.
      test('focus sink search input', function(done) {
        MockInteractions.focus(container.$$('#sink-search-input'));
        setTimeout(function() {
          checkCurrentView(media_router.MediaRouterView.FILTER);
          assertEquals(
              container.$$('#sink-search-input'),
              container.shadowRoot.activeElement);
          done();
        });
      });

      // Tests that the back button in the FILTER view returns |container| to
      // the SINK_LIST view.
      test('filter view back button', function(done) {
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          MockInteractions.tap(
              container.shadowRoot.getElementById('container-header')
                  .shadowRoot.getElementById('back-button'));
          checkCurrentView(media_router.MediaRouterView.SINK_LIST);
          done();
        });
      });

      // Tests that pressing the Escape key in the FILTER view returns
      // |container| to the SINK_LIST view.
      test('filter view escape key', function(done) {
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          pressEscapeOnElement(container);
          checkCurrentView(media_router.MediaRouterView.SINK_LIST);
          done();
        });
      });

      // Tests that pressing the Escape key in the FILTER view when a sink has
      // keyboard focus returns |container| to the SINK_LIST view and focuses
      // the correct sink in the list. We need to check that the same sink
      // remains both focused and selected, where focused refers to the
      // "focused" HTML attribute, and selected refers to the Polymer "selected"
      // attribute of the paper-menu for the sink list.
      test('filter view escape key on menu item', function(done) {
        var focusIndex = 2;
        container.allSinks = fakeSinkList;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          var searchResultsPaperMenu =
              container.$$('#search-results-paper-menu');
          // Use the Polymer method for selecting (and focusing on) a sink
          searchResultsPaperMenu.selectIndex(focusIndex);
          var itemInSearch = searchResultsPaperMenu.querySelectorAll(
              'button.selectable-item')[focusIndex];
          // TODO(crbug.com/608551): This condition handles flakiness around
          // the search item getting focus earlier. If it doesn't get focus,
          // the logic that changes focus from a search item to a sink list
          // item obviously won't do anything.
          if (itemInSearch.focused) {
            var selectedIndexInSearch = searchResultsPaperMenu.selected;
            pressEscapeOnElement(itemInSearch);
            checkCurrentView(media_router.MediaRouterView.SINK_LIST);
            chainOnAnimationPromise(function() {
              var sinkListPaperMenu = container.$$('#sink-list-paper-menu');
              var item = sinkListPaperMenu.querySelectorAll(
                  'button.selectable-item')[focusIndex];

              // Check that the "focused" HTML attribute persists.
              assertTrue(item.focused);
              // Check that the "selected" Polymer attribute persists.
              assertEquals(sinkListPaperMenu.selected, selectedIndexInSearch);
              done();
            });
          } else {
            done();
          }
        });
      });

      // Tests that pressing the Escape key in the FILTER view when a sink does
      // not have keyboard focus returns |container| to the SINK_LIST view and
      // leaves focus where it is.
      test('filter view escape key on menu item other focus', function(done) {
        container.allSinks = fakeSinkList;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          var item = container.$$('#search-results')
                         .querySelectorAll('button.selectable-item')[1];
          var closeButton = container.$['container-header'].$['close-button'];
          closeButton.focus();
          var focusedSuccess = closeButton.focused;
          pressEscapeOnElement(item);
          checkCurrentView(media_router.MediaRouterView.SINK_LIST);
          setTimeout(function() {
            // TODO(crbug.com/608551): This condition handles flakiness around
            // the button initially getting focus. If it doesn't get focus
            // earlier, it obviously shouldn't have it now.
            assertEquals(focusedSuccess, closeButton.focused);
            done();
          });
        });
      });

      // Tests that expected elements are visible when in filter view.
      test('filter view visibility', function(done) {
        checkElementsVisibleWithId([
          'container-header', 'device-missing', 'sink-search', 'sink-list-view'
        ]);
        // Clicking the search icon should transition |container| to FILTER
        // view.
        MockInteractions.tap(container.$$('#sink-search-icon'));
        chainOnAnimationPromise(function() {
          // The search results container is visible, since we are in filter
          // view, but there are no results yet since there are no sinks.
          checkElementsVisibleWithId([
            'container-header', 'device-missing', 'sink-search',
            'search-results-container', 'sink-list-view'
          ]);

          // Adding sinks should populate the search list.
          container.allSinks = fakeSinkList;
          chainOnAnimationPromise(function() {
            checkElementsVisibleWithId([
              'container-header', 'search-results-container', 'search-results',
              'sink-search', 'sink-list-view'
            ]);
            // Typing text that doesn't match any sinks should display a 'no
            // matches' message.
            container.$$('#sink-search-input').value = searchTextNone;
            checkElementsVisibleWithId([
              'container-header', 'search-results-container',
              'no-search-matches', 'sink-search', 'sink-list-view'
            ]);
            // Changing that text to something that matches at least one sink
            // should show the matching sinks again.
            container.$$('#sink-search-input').value = searchTextOne;
            // maybe inside setTimeout
            checkElementsVisibleWithId([
              'container-header', 'search-results-container', 'search-results',
              'sink-search', 'sink-list-view'
            ]);
            // Clicking the back button should leave |searchTextOne| in the
            // input but return to the SINK_LIST view.
            MockInteractions.tap(
                container.shadowRoot.getElementById('container-header')
                    .shadowRoot.getElementById('back-button'));
            chainOnAnimationPromise(function() {
              checkElementsVisibleWithId([
                'container-header', 'sink-search', 'sink-list', 'sink-list-view'
              ]);
              // When the search button is clicked again, the matching sinks
              // should be shown again. This doesn't prove that the matching
              // worked when returning to the FILTER view though, just that it
              // at least shows some sort of sink list as search results.
              MockInteractions.tap(container.$$('#sink-search-icon'));
              chainOnAnimationPromise(function() {
                checkElementsVisibleWithId([
                  'container-header', 'search-results-container',
                  'search-results', 'sink-search', 'sink-list-view'
                ]);

                container.$$('#sink-search-input').value = searchTextNone;
                // Clicking the back button should leave |searchTextNone| in the
                // input but return to the SINK_LIST view.
                MockInteractions.tap(
                    container.shadowRoot.getElementById('container-header')
                        .shadowRoot.getElementById('back-button'));
                chainOnAnimationPromise(function() {
                  checkElementsVisibleWithId([
                    'container-header', 'sink-search', 'sink-list',
                    'sink-list-view'
                  ]);
                  // When the search button is clicked again, there should be no
                  // matches because |searchTextNone| should still be used to
                  // filter.
                  MockInteractions.tap(container.$$('#sink-search-icon'));
                  chainOnAnimationPromise(function() {
                    checkElementsVisibleWithId([
                      'container-header', 'search-results-container',
                      'no-search-matches', 'sink-search', 'sink-list-view'
                    ]);
                    // Pressing the Escape key in FILTER view should return
                    // |container| to SINK_LIST view and not exit the dialog.
                    pressEscapeOnElement(container);
                    chainOnAnimationPromise(function() {
                      checkElementsVisibleWithId([
                        'container-header', 'sink-search', 'sink-list',
                        'sink-list-view'
                      ]);
                      done();
                    });
                  });
                });
              });
            });
          });
        });
      });

      // Tests for expected visible UI when the view is FILTER, and there is
      // a non blocking issue.
      test('filter view visibility non blocking issue', function(done) {
        container.showSinkList_();

        // Set an non-empty sink list.
        container.allSinks = fakeSinkList;

        // Set a non-blocking issue. The issue should be shown.
        container.issue = fakeNonBlockingIssue;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        chainOnAnimationPromise(function() {
          checkElementsVisibleWithId([
            'container-header', 'issue-banner', 'search-results-container',
            'search-results', 'sink-search', 'sink-list-view'
          ]);
          done();
        });
      });

      // Tests that entering filter view with text already in the search input
      // will immediately use that text to filter the sinks. This tests the case
      // that the text matches one sink.
      test('existing search text filters success', function(done) {
        container.allSinks = fakeSinkList;
        container.$$('#sink-search-input').value = searchTextOne;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          var searchResults = container.$$('#search-results')
                                  .querySelectorAll('button.selectable-item');
          assertEquals(1, searchResults.length);
          done();
        });
      });

      // Tests that entering filter view with text already in the search input
      // will immediately use that text to filter the sinks. This tests the case
      // that the text doesn't match any sinks.
      test('existing search text filters fail', function(done) {
        container.allSinks = fakeSinkList;
        container.$$('#sink-search-input').value = searchTextNone;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          var searchResults = container.$$('#search-results')
                                  .querySelectorAll('button.selectable-item');
          assertEquals(0, searchResults.length);
          done();
        });
      });

      // Tests that the text in the search input is not cleared or altered after
      // leaving filter view by pressing the back button in the header.
      test('search text persists back button', function(done) {
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          container.$$('#sink-search-input').value = searchTextAll;

          MockInteractions.tap(
              container.shadowRoot.getElementById('container-header')
                  .shadowRoot.getElementById('back-button'));
          assertEquals(searchTextAll, container.$$('#sink-search-input').value);
          done();
        });
      });

      // Tests that the text in the search input is not cleared or altered after
      // leaving filter view by pressing the Escape key.
      test('search text persists escape key', function(done) {
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          container.$$('#sink-search-input').value = searchTextAll;

          pressEscapeOnElement(container);
          assertEquals(searchTextAll, container.$$('#sink-search-input').value);
          done();
        });
      });
    });
  }

  function registerTestsPart2() {
    suite('MediaRouterContainerFilterPart2', function() {
      suiteSetup(doSuiteSetup);
      setup(doSetup);

      // Tests that the correct number of results are returned in the search
      // results.
      test('search text filters correct number', function(done) {
        container.allSinks = fakeSinkList;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        var searchInput = container.$$('#sink-search-input');
        setTimeout(function() {
          searchInput.value = searchTextAll;
          var searchResults = container.$$('#search-results')
                                  .querySelectorAll('button.selectable-item');
          assertEquals(fakeSinkList.length, searchResults.length);

          searchInput.value = searchTextOne;
          setTimeout(function() {
            var searchResults = container.$$('#search-results')
                                    .querySelectorAll('button.selectable-item');
            assertEquals(1, searchResults.length);

            searchInput.value = searchTextNone;
            setTimeout(function() {
              var searchResults =
                  container.$$('#search-results')
                      .querySelectorAll('button.selectable-item');
              assertEquals(0, searchResults.length);
              done();
            });
          });
        });
      });

      // Tests that the correct item is returned when searching using the first
      // fake sink's name.
      test('search text filters correct text', function(done) {
        container.allSinks = fakeSinkList;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        var testSinkName = fakeSinkList[0].name;
        container.$$('#sink-search-input').value = testSinkName;
        setTimeout(function() {
          var searchResults = container.$$('#search-results')
                                  .querySelectorAll('button.selectable-item');
          assertEquals(1, searchResults.length);
          // This selector works only because there's only one result in the
          // list.
          var searchResultText =
              container.$$('media-router-search-highlighter').text;
          assertEquals(testSinkName.trim(), searchResultText.trim());
          done();
        });
      });

      // Tests that a route can be created from search results the same as in
      // the sink list.
      test('create route from search result without a route', function(done) {
        container.allSinks = fakeSinkList;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          var searchResults = container.$$('#search-results')
                                  .querySelectorAll('button.selectable-item');
          container.addEventListener('create-route', function(data) {
            assertEquals(fakeSinkList[1].id, data.detail.sinkId);
            done();
          });
          MockInteractions.tap(searchResults[1]);
        });
      });

      // Tests that clicking a sink in the search results that already has an
      // associated route will transition |container| to the ROUTE_DETAILS view.
      test('navigate to route details from search result', function(done) {
        container.allSinks = fakeSinkList;
        container.routeList = fakeRouteList;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        chainOnAnimationPromise(function() {
          var searchResults = container.$$('#search-results')
                                  .querySelectorAll('button.selectable-item');
          MockInteractions.tap(searchResults[1]);
          checkCurrentView(media_router.MediaRouterView.ROUTE_DETAILS);
          done();
        });
      });

      // Tests that subtext is shown in filter view the same as the sink list.
      // This is basically a copy of 'initial sink list route text' but in
      // filter view.
      test('subtext displayed in filter view', function(done) {
        // Sink 1 - no sink description, no route -> no subtext
        // Sink 2 - sink description, no route -> subtext = sink description
        // Sink 3 - no sink description, route -> subtext = route description
        // Sink 4 - sink description, route -> subtext = route description
        container.allSinks = [
          new media_router.Sink(
              'sink id 1', 'Sink 1', null, null, media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, [1, 2, 3]),
          new media_router.Sink(
              'sink id 2', 'Sink 2', 'Sink 2 description', null,
              media_router.SinkIconType.CAST, media_router.SinkStatus.ACTIVE,
              [1, 2, 3]),
          new media_router.Sink(
              'sink id 3', 'Sink 3', null, null, media_router.SinkIconType.CAST,
              media_router.SinkStatus.PENDING, [1, 2, 3]),
          new media_router.Sink(
              'sink id 4', 'Sink 4', 'Sink 4 description', null,
              media_router.SinkIconType.CAST, media_router.SinkStatus.PENDING,
              [1, 2, 3])
        ];

        container.routeList = [
          new media_router.Route('id 3', 'sink id 3', 'Title 3', 0, true),
          new media_router.Route('id 4', 'sink id 4', 'Title 4', 1, false),
        ];

        MockInteractions.tap(container.$$('#sink-search-icon'));
        setTimeout(function() {
          var sinkSubtextList =
              container.$$('#search-results').querySelectorAll('.sink-subtext');

          // There will only be 3 sink subtext entries, because Sink 1 does not
          // have any subtext.
          assertEquals(3, sinkSubtextList.length);

          checkElementText(
              container.allSinks[1].description, sinkSubtextList[0]);

          // Route description overrides sink description for subtext.
          checkElementText(
              container.routeList[0].description, sinkSubtextList[1]);

          checkElementText(
              container.routeList[1].description, sinkSubtextList[2]);
          done();
        });
      });

      // Tests that a filter action is reported only if text is entered in the
      // search input and only once for each time the filter view is entered.
      test('report filter action', function(done) {
        var expectReportThen = function(cause, continuation) {
          var internalExpect = function() {
            container.removeEventListener('report-filter', internalExpect);
            chainOnAnimationPromise(continuation);
          };
          container.addEventListener('report-filter', internalExpect);
          cause();
        };
        var expectNoReport = function() {
          assertTrue(false);
        };
        var cause;
        var continuation;

        // Tests that entering filter view and returning to the sink list view
        // without typing any text doesn't report a filter action.
        container.addEventListener('report-filter', expectNoReport);
        MockInteractions.tap(container.$$('#sink-search-icon'));
        chainOnAnimationPromise(function() {
          MockInteractions.tap(
              container.shadowRoot.getElementById('container-header')
                  .shadowRoot.getElementById('back-button'));
          container.removeEventListener('report-filter', expectNoReport);

          // Tests that entering text for the first time in filter view reports
          // a filter action.
          cause = function() {
            container.$$('#sink-search-input').value = 'a';
          };
          continuation = function() {
            // Tests that entering more text in filter view doesn't report a
            // filter action.
            container.addEventListener('report-filter', expectNoReport);
            container.$$('#sink-search-input').value = 'abc';
            MockInteractions.tap(
                container.shadowRoot.getElementById('container-header')
                    .shadowRoot.getElementById('back-button'));
            container.removeEventListener('report-filter', expectNoReport);

            // Tests that entering filter view again with text already in the
            // search input reports a filter action.
            cause = function() {
              MockInteractions.tap(container.$$('#sink-search-icon'));
            };
            continuation = function() {
              // Tests that entering more text when the initial entry to the
              // filter view reported a filter action doesn't report another
              // filter action.
              container.addEventListener('report-filter', expectNoReport);
              container.$$('#sink-search-input').value = 'abcde';
              done();
            };
            expectReportThen(cause, continuation);
          };
          expectReportThen(cause, continuation);
        });
      });

      // Tests that filter view is not entered when switching between windows or
      // tabs with keyboard focus on the search input and the container is
      // currently in sink-list view.
      test('switching window focus does not change view', function(done) {
        container.allSinks = fakeSinkList;
        MockInteractions.tap(container.$$('#sink-search-icon'));
        chainOnAnimationPromise(function() {
          checkCurrentView(media_router.MediaRouterView.FILTER);
          pressEscapeOnElement(container);
          chainOnAnimationPromise(function() {
            var searchInput = container.$$('#sink-search-input');
            checkCurrentView(media_router.MediaRouterView.SINK_LIST);
            assertEquals(container.shadowRoot.activeElement, searchInput);
            var blur = new FocusEvent('blur');
            var focus = new FocusEvent('focus');
            // When this window loses focus, the active element receives a blur
            // event then the window receives a blur event. When the window
            // gains focus again, the window receives a focus event first then
            // the active element receives a focus event. Finally the setTimeout
            // lets these events run and we check the resulting state of the
            // dialog.
            searchInput.dispatchEvent(blur);
            window.dispatchEvent(blur);
            window.dispatchEvent(focus);
            searchInput.dispatchEvent(focus);
            setTimeout(function() {
              checkCurrentView(media_router.MediaRouterView.SINK_LIST);
              done();
            });
          });
        });
      });

      // Tests that compareSearchMatches_ works correctly for zero and one
      // substring matches from the filter text. Earlier, longer matches should
      // be ordered first, in that priority order.
      test('compareSearchMatches_ test single substrings', function(done) {
        var checkEqual = function(left, right) {
          assertEquals(0, container.compareSearchMatches_(left, right));
          assertEquals(0, container.compareSearchMatches_(right, left));
        };
        var checkLess = function(left, right) {
          assertEquals(-1, container.compareSearchMatches_(left, right));
          assertEquals(1, container.compareSearchMatches_(right, left));
        };

        var noMatches = {sinkItem: null, substrings: []};
        var oneMatchSectionSingleChar = {sinkItem: null, substrings: [[0, 0]]};

        checkEqual(noMatches, noMatches);
        checkEqual(oneMatchSectionSingleChar, oneMatchSectionSingleChar);
        checkLess(oneMatchSectionSingleChar, noMatches);

        var oneMatchSectionBeginningLong = {
          sinkItem: null,
          substrings: [[0, 2]]
        };
        var oneMatchSectionBeginningShort = {
          sinkItem: null,
          substrings: [[0, 1]]
        };
        checkEqual(oneMatchSectionBeginningLong, oneMatchSectionBeginningLong);
        checkEqual(
            oneMatchSectionBeginningShort, oneMatchSectionBeginningShort);

        checkLess(oneMatchSectionBeginningLong, oneMatchSectionBeginningShort);

        var oneMatchSectionMiddleLong = {sinkItem: null, substrings: [[2, 4]]};
        var oneMatchSectionMiddleShort = {sinkItem: null, substrings: [[2, 3]]};
        checkEqual(oneMatchSectionMiddleLong, oneMatchSectionMiddleLong);
        checkEqual(oneMatchSectionMiddleShort, oneMatchSectionMiddleShort);

        checkLess(oneMatchSectionMiddleLong, oneMatchSectionMiddleShort);

        var oneMatchSectionEndLong = {sinkItem: null, substrings: [[4, 6]]};
        var oneMatchSectionEndShort = {sinkItem: null, substrings: [[4, 5]]};
        checkEqual(oneMatchSectionEndLong, oneMatchSectionEndLong);
        checkEqual(oneMatchSectionEndShort, oneMatchSectionEndShort);

        checkLess(oneMatchSectionEndLong, oneMatchSectionEndShort);

        // Check beginning < middle < end for both short and long matches.
        checkLess(oneMatchSectionBeginningLong, oneMatchSectionMiddleLong);
        checkLess(oneMatchSectionMiddleLong, oneMatchSectionEndLong);
        checkLess(oneMatchSectionBeginningLong, oneMatchSectionEndLong);
        checkLess(oneMatchSectionBeginningShort, oneMatchSectionMiddleShort);
        checkLess(oneMatchSectionMiddleShort, oneMatchSectionEndShort);
        checkLess(oneMatchSectionBeginningShort, oneMatchSectionEndShort);

        // Check some long/short transitivity
        // i.e. beginning-long < middle-long, middle-long < middle-short, so
        // check that beginning-long < middle-short
        checkLess(oneMatchSectionBeginningLong, oneMatchSectionMiddleShort);
        checkLess(oneMatchSectionBeginningShort, oneMatchSectionMiddleLong);
        checkLess(oneMatchSectionMiddleLong, oneMatchSectionEndShort);
        checkLess(oneMatchSectionMiddleShort, oneMatchSectionEndLong);
        checkLess(oneMatchSectionBeginningLong, oneMatchSectionEndShort);
        checkLess(oneMatchSectionBeginningShort, oneMatchSectionEndLong);

        var oneMatchBeginningOverlap = {sinkItem: null, substrings: [[0, 2]]};
        var oneMatchMiddleOverlap = {sinkItem: null, substrings: [[1, 3]]};
        var oneMatchEndOverlap = {sinkItem: null, substrings: [[2, 4]]};

        checkEqual(oneMatchBeginningOverlap, oneMatchBeginningOverlap);
        checkEqual(oneMatchMiddleOverlap, oneMatchMiddleOverlap);
        checkEqual(oneMatchEndOverlap, oneMatchEndOverlap);

        checkLess(oneMatchBeginningOverlap, oneMatchMiddleOverlap);
        checkLess(oneMatchMiddleOverlap, oneMatchEndOverlap);
        checkLess(oneMatchBeginningOverlap, oneMatchEndOverlap);

        done();
      });

      // Tests that compareSearchMatches_ works correctly for one or more
      // substring matches from the filter text. Earlier, longer matches should
      // be ordered first, in that priority order.
      test('compareSearchMatches_ test multiple substrings', function(done) {
        var checkEqual = function(left, right) {
          assertEquals(0, container.compareSearchMatches_(left, right));
          assertEquals(0, container.compareSearchMatches_(right, left));
        };
        var checkLess = function(left, right) {
          assertEquals(-1, container.compareSearchMatches_(left, right));
          assertEquals(1, container.compareSearchMatches_(right, left));
        };

        // Variables are named by number of substring elements followed by their
        // sort order as X_Y where they should be sorted in ascending order by
        // X.Y. For example: 1_1 < 1_2 < 2_1.
        var threeMatches1_1 = {
          sinkItem: null,
          substrings: [[0, 2], [4, 5], [7, 9]],
        };
        var threeMatches1_2 = {
          sinkItem: null,
          substrings: [[0, 2], [4, 5], [7, 8]],
        };
        var threeMatches1_3 = {
          sinkItem: null,
          substrings: [[0, 2], [4, 5], [8, 9]],
        };
        var threeMatches1_4 = {
          sinkItem: null,
          substrings: [[0, 2], [4, 4], [6, 8]],
        };
        var twoMatches2_1 = {
          sinkItem: null,
          substrings: [[0, 2], [4, 4]],
        };
        var twoMatches2_2 = {
          sinkItem: null,
          substrings: [[0, 1], [3, 5]],
        };
        var twoMatches2_3 = {
          sinkItem: null,
          substrings: [[0, 1], [4, 6]],
        };
        var twoMatches2_4 = {
          sinkItem: null,
          substrings: [[0, 1], [4, 5]],
        };
        var threeMatches2_5 = {
          sinkItem: null,
          substrings: [[0, 1], [4, 4], [6, 9]],
        };
        var oneMatch3_1 = {
          sinkItem: null,
          substrings: [[0, 1]],
        };
        var oneMatch3_2 = {
          sinkItem: null,
          substrings: [[0, 0]],
        };

        var orderedMatches = [
          threeMatches1_1,
          threeMatches1_2,
          threeMatches1_3,
          threeMatches1_4,
          twoMatches2_1,
          twoMatches2_2,
          twoMatches2_3,
          twoMatches2_4,
          threeMatches2_5,
          oneMatch3_1,
          oneMatch3_2,
        ];

        for (var i = 0; i < orderedMatches.length; ++i) {
          checkEqual(orderedMatches[i], orderedMatches[i]);
        }
        for (var i = 0; i < orderedMatches.length - 1; ++i) {
          checkLess(orderedMatches[i], orderedMatches[i + 1]);
        }
        // Check some transitivity.
        for (var i = 0; i < orderedMatches.length - 2; ++i) {
          checkLess(orderedMatches[i], orderedMatches[i + 2]);
        }
        for (var i = 0; i < orderedMatches.length - 3; ++i) {
          checkLess(orderedMatches[i], orderedMatches[i + 3]);
        }

        done();
      });

      // Tests that computeSearchMatches_ correctly computes the matching
      // substrings of a sink name from search text.
      test('computeSearchMatches_ test', function(done) {
        var sinkName = '012345 789';
        var checkMatches = function(searchText, answer) {
          proposed = container.computeSearchMatches_(searchText, sinkName);

          if (answer == null || proposed == null) {
            assertEquals(answer, proposed);
            return;
          }
          assertEquals(answer.length, proposed.length);
          for (var i = 0; i < proposed.length; ++i) {
            assertEquals(answer[i].length, proposed[i].length);
            for (var j = 0; j < proposed[i].length; ++j) {
              assertEquals(answer[i][j], proposed[i][j]);
            }
          }
        };

        // Check search text against |sinkName| for correct match output.
        checkMatches('', []);
        checkMatches('a', null);
        checkMatches('0', [[0, 0]]);
        checkMatches('1', [[1, 1]]);
        checkMatches('012', [[0, 2]]);
        checkMatches('03', [[0, 0], [3, 3]]);
        checkMatches('210', null);
        checkMatches('01345789', [[0, 1], [3, 5], [7, 9]]);
        checkMatches('024', [[0, 0], [2, 2], [4, 4]]);
        checkMatches('09a', null);
        checkMatches(' ', [[6, 6]]);
        checkMatches('45 ', [[4, 6]]);
        checkMatches(' 78', [[6, 8]]);
        checkMatches('45 7', [[4, 7]]);
        checkMatches('  ', null);
        checkMatches('12 89', [[1, 2], [6, 6], [8, 9]]);

        done();
      });

      // Tests that computeSinkMatchingText_ correctly splits a sink name into
      // |plainText| and |highlightedText| arrays given a sink name and an array
      // of match indices.
      test('computeSinkMatchingText_ test', function(done) {
        var sinkName = '012345 789';
        var sink = new media_router.Sink(
            'id', sinkName, null, null, media_router.SinkIconType.CAST,
            media_router.SinkStatus.ACTIVE, 0);
        var checkMatches = function(matchesAndAnswers) {
          var matches = matchesAndAnswers.matches;
          var plainText = matchesAndAnswers.plainText;
          var highlightedText = matchesAndAnswers.highlightedText;

          var proposed = container.computeSinkMatchingText_(
              {sinkItem: sink, substrings: matches});
          assertEquals(plainText.length, proposed.plainText.length);
          assertEquals(highlightedText.length, proposed.highlightedText.length);
          for (var i = 0; i < plainText.length; ++i) {
            assertEquals(plainText[i], proposed.plainText[i]);
          }
          for (var i = 0; i < highlightedText.length; ++i) {
            assertEquals(highlightedText[i], proposed.highlightedText[i]);
          }
        };

        // Check that |sinkName| is correctly partitioned by |matches|.
        var matchesAndAnswers1 = {
          matches: null,
          plainText: ['012345 789'],
          highlightedText: [null]
        };
        checkMatches(matchesAndAnswers1);

        var matchesAndAnswers2 = {
          matches: [],
          plainText: ['012345 789'],
          highlightedText: [null]
        };
        checkMatches(matchesAndAnswers2);

        var matchesAndAnswers3 = {
          matches: [[0, 0]],
          plainText: [null, '12345 789'],
          highlightedText: ['0', null]
        };
        checkMatches(matchesAndAnswers3);

        var matchesAndAnswers4 = {
          matches: [[9, 9]],
          plainText: ['012345 78'],
          highlightedText: ['9']
        };
        checkMatches(matchesAndAnswers4);

        var matchesAndAnswers5 = {
          matches: [[1, 1]],
          plainText: ['0', '2345 789'],
          highlightedText: ['1', null]
        };
        checkMatches(matchesAndAnswers5);

        var matchesAndAnswers6 = {
          matches: [[1, 2], [4, 6]],
          plainText: ['0', '3', '789'],
          highlightedText: ['12', '45 ', null]
        };
        checkMatches(matchesAndAnswers6);

        var matchesAndAnswers7 = {
          matches: [[0, 3], [7, 9]],
          plainText: [null, '45 '],
          highlightedText: ['0123', '789'],
        };
        checkMatches(matchesAndAnswers7);

        var matchesAndAnswers8 = {
          matches: [[4, 6], [9, 9]],
          plainText: ['0123', '78'],
          highlightedText: ['45 ', '9']
        };
        checkMatches(matchesAndAnswers8);

        var matchesAndAnswers9 = {
          matches: [[0, 1], [3, 4], [6, 7], [9, 9]],
          plainText: [null, '2', '5', '8'],
          highlightedText: ['01', '34', ' 7', '9'],
        };
        checkMatches(matchesAndAnswers9);

        var matchesAndAnswers10 = {
          matches: [[0, 1], [3, 4], [6, 6], [9, 9]],
          plainText: [null, '2', '5', '78'],
          highlightedText: ['01', '34', ' ', '9'],
        };
        checkMatches(matchesAndAnswers10);

        var matchesAndAnswers11 = {
          matches: [[5, 7]],
          plainText: ['01234', '89'],
          highlightedText: ['5 7', null],
        };
        checkMatches(matchesAndAnswers11);

        done();
      });
    });
  }

  return {
    registerTestsPart1: registerTestsPart1,
    registerTestsPart2: registerTestsPart2,
  };
});
