// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for media-router-container that focus on the
 * cast mode list.
 */
cr.define('media_router_container_cast_mode_list', function() {
  function registerTests() {
    suite('MediaRouterContainerCastModeList', function() {
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
      var fakeBlockingIssue;

      /**
       * The list of CastModes to show.
       * @type {!Array<!media_router.CastMode>}
       */
      var fakeCastModeList = [];

      /**
       * The list of CastModes to show with non-PRESENTATION modes only.
       * @type {!Array<!media_router.CastMode>}
       */
      var fakeCastModeListWithNonPresentationModesOnly = [];

      /**
       * The list of CastModes to show with PRESENTATION mode forced.
       * @type {!Array<!media_router.CastMode>}
       */
      var fakeCastModeListWithPresentationModeForced = [];

      /**
       * The list of CastModes including local media, which is just local file
       * at the moement.
       * @type {!Array<!media_router.CastMode>}
       */
      var fakeCastModeListWithLocalMedia = [];

      /**
       * The blocking issue to show.
       * @type {?media_router.Issue}
       */
      var fakeNonBlockingIssue;

      /**
       * The list of available sinks.
       * @type {!Array<!media_router.Sink>}
       */
      var fakeSinkList = [];

      // Import media_router_container.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://media-router/elements/media_router_container/' +
            'media_router_container.html');
      });

      setup(function(done) {
        PolymerTest.clearBody();
        // Initialize a media-router-container before each test.
        container = document.createElement('media-router-container');
        container.get = function(strName) {
          return this.$[strName];
        };

        document.body.appendChild(container);

        // Get common functions and variables.
        var testBase = media_router_container_test_base.init(container);

        checkCurrentView = testBase.checkCurrentView;
        checkElementsVisibleWithId = testBase.checkElementsVisibleWithId;
        checkElementText = testBase.checkElementText;
        fakeBlockingIssue = testBase.fakeBlockingIssue;
        fakeCastModeList = testBase.fakeCastModeList;
        fakeCastModeListWithNonPresentationModesOnly =
            testBase.fakeCastModeListWithNonPresentationModesOnly;
        fakeCastModeListWithPresentationModeForced =
            testBase.fakeCastModeListWithPresentationModeForced;
        fakeCastModeListWithLocalMedia =
            testBase.fakeCastModeListWithLocalMedia;
        fakeNonBlockingIssue = testBase.fakeNonBlockingIssue;
        fakeSinkList = testBase.fakeSinkList;

        container.castModeList = testBase.fakeCastModeList;

        // Allow for the media router container to be created, attached, and
        // listeners registered in an afterNextRender() call.
        Polymer.RenderStatus.afterNextRender(this, done);
      });

      // Container remains in auto mode even if the cast mode list changed.
      test('cast mode list updated in auto mode', function(done) {
        assertEquals(
            media_router.AUTO_CAST_MODE.description, container.headerText);
        assertEquals(
            media_router.CastModeType.AUTO, container.shownCastModeValue_);
        assertFalse(container.userHasSelectedCastMode_);

        container.castModeList = fakeCastModeList.slice(1);
        setTimeout(function() {
          assertEquals(
              media_router.AUTO_CAST_MODE.description, container.headerText);
          assertEquals(
              media_router.CastModeType.AUTO, container.shownCastModeValue_);
          assertFalse(container.userHasSelectedCastMode_);
          done();
        });
      });

      // Tests that |container| returns to SINK_LIST view and arrow drop icon
      // toggles after a cast mode is selected.
      test('select cast mode', function(done) {
        container.castModeList = fakeCastModeListWithNonPresentationModesOnly;

        MockInteractions.tap(
            container.get('container-header').$['arrow-drop-icon']);
        checkCurrentView(media_router.MediaRouterView.CAST_MODE_LIST);

        setTimeout(function() {
          var castModeList = container.$$('#cast-mode-list')
                                 .querySelectorAll('button.selectable-item');
          MockInteractions.tap(castModeList[1]);
          checkCurrentView(media_router.MediaRouterView.SINK_LIST);
          done();
        });
      });

      // Tests that clicking on the drop down icon will toggle |container|
      // between SINK_LIST and CAST_MODE_LIST views.
      test('click drop down icon', function() {
        checkCurrentView(media_router.MediaRouterView.SINK_LIST);

        MockInteractions.tap(
            container.get('container-header').$['arrow-drop-icon']);
        checkCurrentView(media_router.MediaRouterView.CAST_MODE_LIST);

        MockInteractions.tap(
            container.get('container-header').$['arrow-drop-icon']);
        checkCurrentView(media_router.MediaRouterView.SINK_LIST);
      });

      // Tests the header text. Choosing a cast mode updates the header text.
      test('header text with cast mode selected', function(done) {
        assertEquals(
            loadTimeData.getString('selectCastModeHeaderText'),
            container.i18n('selectCastModeHeaderText'));

        // The container is currently in auto cast mode, since we have not
        // picked a cast mode explicitly, and the sinks is not compatible
        // with exactly one cast mode.
        assertEquals(
            media_router.AUTO_CAST_MODE.description, container.headerText);
        assertFalse(container.userHasSelectedCastMode_);

        container.castModeList = fakeCastModeListWithNonPresentationModesOnly;

        // Switch to cast mode list view.
        MockInteractions.tap(
            container.get('container-header').$['arrow-drop-icon']);
        setTimeout(function() {
          var castModeList = container.$$('#cast-mode-list')
                                 .querySelectorAll('button.selectable-item');
          assertEquals(
              fakeCastModeListWithNonPresentationModesOnly.length,
              castModeList.length);
          for (var i = 0; i < castModeList.length; i++) {
            MockInteractions.tap(castModeList[i]);

            assertEquals(
                fakeCastModeListWithNonPresentationModesOnly[i].description,
                container.headerText);

            checkElementText(
                fakeCastModeListWithNonPresentationModesOnly[i].description,
                castModeList[i]);
          }

          done();
        });
      });

      // Tests the header text when updated with a cast mode list with a mix of
      // PRESENTATION and non-PRESENTATION cast modes.
      test('cast modes with one presentation mode', function(done) {
        container.castModeList = fakeCastModeList;

        // Switch to cast mode list view.
        MockInteractions.tap(
            container.get('container-header').$['arrow-drop-icon']);
        setTimeout(function() {
          var castModeList = container.$$('#cast-mode-list')
                                 .querySelectorAll('button.selectable-item');

          for (var i = 0; i < fakeCastModeList.length; i++) {
            MockInteractions.tap(castModeList[i]);
            if (fakeCastModeList[i].type ==
                media_router.CastModeType.PRESENTATION) {
              assertEquals(
                  fakeCastModeList[i].description, container.headerText);

              checkElementText(fakeCastModeList[i].host, castModeList[i]);
            } else {
              assertEquals(
                  fakeCastModeList[i].description, container.headerText);
              checkElementText(
                  fakeCastModeList[i].description, castModeList[i]);
            }
          }

          done();
        });
      });

      // Tests that pseudo sinks are ignored for the purpose of computing
      // which cast mode to show.
      test('cast modes not affected by pseudo sink', function(done) {
        assertEquals(
            media_router.CastModeType.AUTO, container.shownCastModeValue_);
        container.castModeList = fakeCastModeList;
        var sink = new media_router.Sink(
            'pseudo-sink-id', 'Pseudo sink', null, null,
            media_router.SinkIconType.GENERIC, media_router.SinkStatus.ACTIVE,
            /* DESKTOP */ 0x4);
        sink.isPseudoSink = true;
        container.allSinks = [sink];

        setTimeout(function() {
          assertEquals(
              media_router.CastModeType.AUTO, container.shownCastModeValue_);
          done();
        });
      });

      // Tests for expected visible UI when the view is CAST_MODE_LIST.
      test('cast mode list state visibility', function(done) {
        container.showCastModeList_();
        setTimeout(function() {
          checkElementsVisibleWithId(
              ['cast-mode-list', 'container-header', 'device-missing']);

          // Set a non-blocking issue. The issue should be visible.
          container.issue = fakeNonBlockingIssue;
          setTimeout(function() {
            checkElementsVisibleWithId([
              'cast-mode-list', 'container-header', 'device-missing',
              'issue-banner'
            ]);

            // Set a blocking issue. The cast mode list should not be displayed.
            container.issue = fakeBlockingIssue;
            setTimeout(function() {
              checkElementsVisibleWithId(
                  ['container-header', 'device-missing', 'issue-banner']);
              done();
            });
          });
        });
      });

      // If the container is not in auto mode, and the mode it is currently in
      // no longer exists in the list of cast modes, then switch back to auto
      // mode.
      test('cast mode list updated in selected cast mode', function(done) {
        assertEquals(
            media_router.AUTO_CAST_MODE.description, container.headerText);
        assertEquals(
            media_router.CastModeType.AUTO, container.shownCastModeValue_);
        assertFalse(container.userHasSelectedCastMode_);

        MockInteractions.tap(
            container.get('container-header').$['arrow-drop-icon']);
        setTimeout(function() {
          var castModeList = container.$$('#cast-mode-list')
                                 .querySelectorAll('button.selectable-item');
          MockInteractions.tap(castModeList[0]);
          setTimeout(function() {
            assertEquals(fakeCastModeList[0].description, container.headerText);
            assertEquals(
                fakeCastModeList[0].type, container.shownCastModeValue_);

            assertTrue(container.userHasSelectedCastMode_);

            container.castModeList = fakeCastModeList.slice(1);
            setTimeout(function() {
              assertEquals(
                  media_router.AUTO_CAST_MODE.description,
                  container.headerText);
              assertEquals(
                  media_router.CastModeType.AUTO,
                  container.shownCastModeValue_);
              assertFalse(container.userHasSelectedCastMode_);
              done();
            });
          });
        });
      });

      // Tests that after a different cast mode is selected, the sink list will
      // change based on the sinks compatibility with the new cast mode.
      test('changing cast mode changes sink list', function(done) {
        container.allSinks = fakeSinkList;

        MockInteractions.tap(
            container.get('container-header').$['arrow-drop-icon']);
        setTimeout(function() {
          var castModeList = container.$$('#cast-mode-list')
                                 .querySelectorAll('button.selectable-item');
          MockInteractions.tap(castModeList[0]);
          assertEquals(fakeCastModeList[0].description, container.headerText);

          setTimeout(function() {
            var sinkList = container.shadowRoot.getElementById('sink-list');
            // The sink list is hidden because none of the sinks in
            // fakeSinkList is compatible with cast mode 0.
            assertEquals('none', window.getComputedStyle(sinkList).display);
            MockInteractions.tap(castModeList[2]);
            assertEquals(fakeCastModeList[2].description, container.headerText);

            setTimeout(function() {
              var sinkList = container.shadowRoot.getElementById('sink-list');
              var sinkListElements =
                  sinkList.querySelectorAll('button.selectable-item');
              assertNotEquals(
                  'none', window.getComputedStyle(sinkList).display);
              assertEquals(3, sinkListElements.length);
              done();
            });
          });
        });
      });

      // When a forced cast mode it set, it is used.
      test('cast mode list respects forced mode', function(done) {
        container.allSinks = [
          new media_router.Sink(
              'sink id 1', 'Sink 1', null, null, media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, 0x1),
          new media_router.Sink(
              'sink id 2', 'Sink 2', null, null, media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, 0x1 | 0x2),
          new media_router.Sink(
              'sink id 3', 'Sink 3', null, null, media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, 0x2)
        ];
        container.castModeList = fakeCastModeListWithPresentationModeForced;
        MockInteractions.tap(
            container.$['container-header'].$['arrow-drop-icon']);
        setTimeout(function() {
          assertEquals(
              media_router.CastModeType.PRESENTATION,
              container.shownCastModeValue_);
          assertEquals('Cast google.com', container.headerText);
          assertFalse(container.userHasSelectedCastMode_);

          var sinkList = container.shadowRoot.getElementById('sink-list')
                             .querySelectorAll('button.selectable-item');

          // The sink list contains only sinks compatible with PRESENTATION
          // mode.
          assertEquals(2, sinkList.length);
          checkElementText('Sink 1', sinkList[0]);
          checkElementText('Sink 2', sinkList[1]);
          done();
        });
      });

      // Tests that the sink list does not contain any sinks that are not
      // compatible with the selected cast mode and are not associated with a
      // route.
      test('sink list in user selected cast mode', function(done) {
        var newSinks = [
          new media_router.Sink(
              'sink id 10', 'Sink 10', null, null,
              media_router.SinkIconType.CAST, media_router.SinkStatus.ACTIVE,
              0x4 | 0x8),
          new media_router.Sink(
              'sink id 20', 'Sink 20', null, null,
              media_router.SinkIconType.CAST, media_router.SinkStatus.ACTIVE,
              0x2 | 0x4 | 0x8),
          new media_router.Sink(
              'sink id 30', 'Sink 30', null, null,
              media_router.SinkIconType.CAST, media_router.SinkStatus.PENDING,
              0x4 | 0x8),
        ];

        container.allSinks = newSinks;
        container.routeList = [
          new media_router.Route(
              'id 1', 'sink id 30', 'Title 1', 1, false, false),
        ];

        setTimeout(function() {
          var sinkList = container.shadowRoot.getElementById('sink-list')
                             .querySelectorAll('button.selectable-item');

          // Since we haven't selected a cast mode, we don't filter sinks.
          assertEquals(3, sinkList.length);

          MockInteractions.tap(
              container.get('container-header').$['arrow-drop-icon']);
          setTimeout(function() {
            // Cast mode 1 is selected, and the sink list is filtered.
            var castModeList = container.$$('#cast-mode-list')
                                   .querySelectorAll('button.selectable-item');
            MockInteractions.tap(castModeList[1]);
            assertEquals(fakeCastModeList[1].description, container.headerText);
            assertEquals(
                fakeCastModeList[1].type, container.shownCastModeValue_);

            setTimeout(function() {
              var sinkList = container.shadowRoot.getElementById('sink-list')
                                 .querySelectorAll('button.selectable-item');

              // newSinks[0] got filtered out since it is not compatible with
              // cast mode 1.
              // 'Sink 20' should be on the list because it contains the
              // selected cast mode. (sinkList[0] = newSinks[1])
              // 'Sink 30' should be on the list because it has a route.
              // (sinkList[1] = newSinks[2])
              assertEquals(2, sinkList.length);
              checkElementText(newSinks[1].name, sinkList[0]);

              // |sinkList[1]| contains route title in addition to sink name.
              assertTrue(sinkList[1].textContent.trim().startsWith(
                  newSinks[2].name.trim()));

              // Cast mode is not switched back even if there are no sinks
              // compatible with selected cast mode, because we explicitly
              // selected that cast mode.
              container.allSinks = [];
              setTimeout(function() {
                assertEquals(
                    fakeCastModeList[1].description, container.headerText);
                assertEquals(
                    fakeCastModeList[1].type, container.shownCastModeValue_);

                // The sink list is hidden since there are no compatible sinks.
                var sinkList = container.shadowRoot.getElementById('sink-list');
                assertEquals('none', window.getComputedStyle(sinkList).display);
                done();
              });
            });
          });
        });
      });

      // Tests that the header is set appropriately when files are selected
      // one after the other.
      test('cast to sink with existing route', function(done) {
        container.castModeList = fakeCastModeListWithLocalMedia;

        var fileName1 = 'file1';
        var fileName2 = 'file2';

        container.onFileDialogSuccess(fileName1);
        setTimeout(function() {
          assertTrue(container.headerText.includes(fileName1));
          container.onFileDialogSuccess(fileName2);
          setTimeout(function() {
            assertTrue(container.headerText.includes(fileName2));
            assertFalse(container.headerText.includes(fileName1));
            done();
          });
        });
      });

      // Tests that the 'cast' button is shown in the route details view when
      // the sink for the current route is compatible with the user-selected
      // cast mode.
      test('cast to sink with existing route', function(done) {
        var newSinks = [
          new media_router.Sink(
              'sink id 10', 'Sink 10', null, null,
              media_router.SinkIconType.CAST, media_router.SinkStatus.ACTIVE,
              0x2 | 0x4 | 0x8),
        ];

        container.allSinks = newSinks;
        container.routeList = [
          new media_router.Route(
              'id 1', 'sink id 10', 'Title 1', 1, false, false),
        ];

        setTimeout(function() {
          var sinkList = container.shadowRoot.getElementById('sink-list')
                             .querySelectorAll('button.selectable-item');

          MockInteractions.tap(
              container.get('container-header').$['arrow-drop-icon']);
          setTimeout(function() {
            // Cast mode 1 is selected, and the sink list is filtered.
            var castModeList = container.$$('#cast-mode-list')
                                   .querySelectorAll('button.selectable-item');
            MockInteractions.tap(castModeList[1]);

            setTimeout(function() {
              var sinkList = container.shadowRoot.getElementById('sink-list')
                                 .querySelectorAll('button.selectable-item');

              MockInteractions.tap(sinkList[0]);
              setTimeout(function() {
                assertFalse(container.shadowRoot.getElementById('route-details')
                                .shadowRoot
                                .getElementById('start-casting-to-route-button')
                                .hasAttribute('hidden'));
                done();
              });
            });
          });
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
