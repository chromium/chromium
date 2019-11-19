// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests the Most Visited iframe on the local NTP.
 */

/**
 * Most Visited's object for test and setup functions.
 */
test.mostVisited = {};

/**
 * ID of the Most Visited container.
 * @const {string}
 */
test.mostVisited.MOST_VISITED = 'most-visited';

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
test.mostVisited.CLASSES = {
  GRID_TILE: 'grid-tile',
  GRID_TILE_CONTAINER: 'grid-tile-container',
  REORDER: 'reorder',
  REORDERING: 'reordering',
};

/**
 * Utility to mock out parts of the DOM.
 * @type {Replacer}
 */
test.mostVisited.stubs = new Replacer();

/**
 * The MostVisited object.
 * @type {?Object}
 */
test.mostVisited.mostvisited = null;

/**
 * The MostVisited grid.
 * @type {?Grid}
 */
test.mostVisited.grid = null;

/**
 * The restricted id of the item that will be reordered.
 * @type {number}
 */
test.mostVisited.reorderRid = -1;

/**
 * The new index to reorder the item to.
 * @type {number}
 */
test.mostVisited.reorderNewIndex = -1;

/**
 * Set up the text DOM and test environment.
 */
test.mostVisited.setUp = function() {
  setUpPage('most-visited-template');

  // Reset variable values.
  test.mostVisited.reorderRid = -1;
  test.mostVisited.reorderNewIndex = -1;
  document.documentElement.dir = null;

  // Mock chrome.embeddedSearch.newTabPage functions.
  test.mostVisited.stubs.replace(
      chrome.embeddedSearch.newTabPage, 'reorderCustomLink',
      (rid, newIndex) => {
        test.mostVisited.reorderRid = rid;
        test.mostVisited.reorderNewIndex = newIndex;
      });

  test.mostVisited.mostvisited = test.mostVisited.init();
  test.mostVisited.grid = new test.mostVisited.mostvisited.Grid();
};

/**
 * Tests if the grid creates the correct tile layout.
 */
test.mostVisited.testGridLayout = function() {
  window.innerWidth = 100;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 6,
    maxTilesPerRow: 5,
    maxTiles: 10
  };

  // Create a grid with 1 tile.
  let container = document.createElement('div');
  let expectedLayout = ['translate(0px, 0px)'];
  test.mostVisited.initGrid(container, params, 1);
  assertEquals('10px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with a full row.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(30px, 0px)', 'translate(40px, 0px)'
  ];
  test.mostVisited.initGrid(container, params, 5);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with balanced rows. There should be 3 tiles per row.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(0px, 10px)', 'translate(10px, 10px)', 'translate(20px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 6);
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with two uneven rows. The second row should be offset by a
  // half tile.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(30px, 0px)', 'translate(5px, 10px)', 'translate(15px, 10px)',
    'translate(25px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 7);
  assertEquals('40px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with max rows.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(30px, 0px)', 'translate(40px, 0px)', 'translate(0px, 10px)',
    'translate(10px, 10px)', 'translate(20px, 10px)', 'translate(30px, 10px)',
    'translate(40px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 10);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
};

/**
 * Tests if the grid creates the correct tile layout in RTL. The (x,y) positions
 * should be the same as LTR except with a negative x value.
 */
test.mostVisited.testGridLayoutRtl = function() {
  document.documentElement.dir = 'rtl';  // Enable RTL.
  window.innerWidth = 100;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 6,
    maxTilesPerRow: 5,
    maxTiles: 10
  };

  // Create a grid with 1 tile.
  let container = document.createElement('div');
  let expectedLayout = ['translate(0px, 0px)'];
  test.mostVisited.initGrid(container, params, 1);
  assertEquals('10px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with a full row.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(-30px, 0px)', 'translate(-40px, 0px)'
  ];
  test.mostVisited.initGrid(container, params, 5);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with balanced rows. There should be 3 tiles per row.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(0px, 10px)', 'translate(-10px, 10px)', 'translate(-20px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 6);
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with two uneven rows. The second row should be offset by a
  // half tile.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(-30px, 0px)', 'translate(-5px, 10px)', 'translate(-15px, 10px)',
    'translate(-25px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 7);
  assertEquals('40px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with max rows.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(-30px, 0px)', 'translate(-40px, 0px)', 'translate(0px, 10px)',
    'translate(-10px, 10px)', 'translate(-20px, 10px)',
    'translate(-30px, 10px)', 'translate(-40px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 10);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
};

/**
 * Tests if the grid resizes correctly according to window size.
 */
test.mostVisited.testGridResize = function() {
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 4,
    maxTilesPerRow: 3,
    maxTiles: 6
  };

  window.innerWidth = 30;
  // Create a grid with max rows.
  let container = document.createElement('div');
  let expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(0px, 10px)', 'translate(10px, 10px)', 'translate(20px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 6);
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);

  // Shrink the window so that only 4 tiles are visible.
  window.innerWidth = 20;
  // The last two tiles will still increment their x coordinate, but they will
  // be hidden.
  let expectedLayoutShrink = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(0px, 10px)',
    'translate(10px, 10px)', 'translate(20px, 10px)', 'translate(30px, 10px)'
  ];
  test.mostVisited.grid.onResize();
  assertEquals('20px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayoutShrink);
  test.mostVisited.assertVisibility(container, 2);

  // Expand the window so that all tiles are visible.
  window.innerWidth = 100;
  test.mostVisited.grid.onResize();
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);
};

/**
 * Tests if the grid resizes correctly in RTL according to window size. The
 * (x,y) positions should be the same as LTR except with a negative x value.
 */
test.mostVisited.testGridResizeRtl = function() {
  document.documentElement.dir = 'rtl';  // Enable RTL.
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 4,
    maxTilesPerRow: 3,
    maxTiles: 6
  };

  window.innerWidth = 30;
  // Create a grid with max rows.
  let container = document.createElement('div');
  let expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(0px, 10px)', 'translate(-10px, 10px)', 'translate(-20px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 6);
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);

  // Shrink the window so that only 4 tiles are visible.
  window.innerWidth = 20;
  // The last two tiles will still increment their x coordinate, but they will
  // be hidden.
  let expectedLayoutShrink = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(0px, 10px)',
    'translate(-10px, 10px)', 'translate(-20px, 10px)', 'translate(-30px, 10px)'
  ];
  test.mostVisited.grid.onResize();
  assertEquals('20px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayoutShrink);
  test.mostVisited.assertVisibility(container, 2);

  // Expand the window so that all tiles are visible.
  window.innerWidth = 100;
  test.mostVisited.grid.onResize();
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);
};

/**
 * Tests if the grid rebalances correctly when the window is resized.
 */
test.mostVisited.testGridResizeRebalance = function() {
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 6,
    maxTilesPerRow: 5,
    maxTiles: 10
  };

  window.innerWidth = 50;
  // Create a grid a full row.
  let container = document.createElement('div');
  let expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(30px, 0px)', 'translate(40px, 0px)'
  ];
  test.mostVisited.initGridWithAdd(container, params, 5);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);

  // Shrink the window so that only 4 tiles can fit in a row.
  window.innerWidth = 40;
  // The tiles should rebalance.
  let expectedLayoutRebalance = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(5px, 10px)', 'translate(15px, 10px)'
  ];
  test.mostVisited.grid.onResize();
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayoutRebalance);
  test.mostVisited.assertVisibility(container, 0);
};

/**
 * Tests if all tiles except the add button can be reordered.
 */
test.mostVisited.testReorderStart = function() {
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 6,
    maxTilesPerRow: 5,
    maxTiles: 10,
    enableReorder: true
  };

  // Create a grid with max rows and an add shortcut button.
  let container = document.createElement('div');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGridWithAdd(container, params, 10);

  const dragStart = test.mostVisited.mockDragStart();
  const dragEnd = new Event('dragend');

  const tiles = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER);
  assertEquals(10, tiles.length);
  // Test that we can reorder all tiles except for the add button.
  for (let i = 0; i < 9; i++) {
    let tile = tiles[i];
    assertEquals('false', tile.getAttribute('add'));
    assertEquals(i, Number(tile.getAttribute('rid')));
    assertTrue(tile.firstChild.draggable);

    assertFalse(tile.classList.contains(test.mostVisited.CLASSES.REORDER));
    assertFalse(
        document.body.classList.contains(test.mostVisited.CLASSES.REORDERING));

    // Start the reorder flow.
    tile.firstChild.dispatchEvent(dragStart);

    assertTrue(tile.classList.contains(test.mostVisited.CLASSES.REORDER));
    assertTrue(
        document.body.classList.contains(test.mostVisited.CLASSES.REORDERING));

    // Stop the reorder flow.
    document.dispatchEvent(dragEnd);

    assertFalse(tile.classList.contains(test.mostVisited.CLASSES.REORDER));
    assertFalse(
        document.body.classList.contains(test.mostVisited.CLASSES.REORDERING));
  }

  // Try and fail to reorder the add button.
  let addButton = container.children[9];
  assertEquals('true', addButton.getAttribute('add'));
  assertFalse(addButton.firstChild.draggable);
  addButton.firstChild.dispatchEvent(dragStart);

  assertFalse(addButton.classList.contains(test.mostVisited.CLASSES.REORDER));
  assertFalse(
      document.body.classList.contains(test.mostVisited.CLASSES.REORDERING));
};

/**
 * Tests if the tiles can be reordered using touch.
 */
test.mostVisited.testReorderStartTouch = function() {
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 6,
    maxTilesPerRow: 5,
    maxTiles: 10,
    enableReorder: true
  };

  // Create a grid with 1 tile and an add shortcut button.
  let container = document.createElement('div');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGridWithAdd(container, params, 2);

  const touchStart = new Event('touchstart');
  touchStart.changedTouches = [{pageX: 0, pageY: 0}];  // Point to some spot.
  const touchMove = new Event('touchmove');
  touchMove.changedTouches = [{pageX: 0, pageY: 0}];  // Point to some spot.
  const touchEnd = new Event('touchend');

  // Test that we can reorder a tile.
  const tile = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER)[0];
  assertEquals('false', tile.getAttribute('add'));
  assertEquals(0, Number(tile.getAttribute('rid')));

  assertFalse(tile.classList.contains(test.mostVisited.CLASSES.REORDER));
  assertFalse(
      document.body.classList.contains(test.mostVisited.CLASSES.REORDERING));

  // Start the reorder flow.
  tile.firstChild.dispatchEvent(touchStart);
  tile.firstChild.dispatchEvent(touchMove);

  assertTrue(tile.classList.contains(test.mostVisited.CLASSES.REORDER));
  assertTrue(
      document.body.classList.contains(test.mostVisited.CLASSES.REORDERING));

  // Stop the reorder flow.
  tile.firstChild.dispatchEvent(touchEnd);

  assertFalse(tile.classList.contains(test.mostVisited.CLASSES.REORDER));
  assertFalse(
      document.body.classList.contains(test.mostVisited.CLASSES.REORDERING));

  // Try and fail to reorder the add button.
  let addButton = container.children[1];
  assertEquals('true', addButton.getAttribute('add'));
  addButton.firstChild.dispatchEvent(touchStart);
  addButton.firstChild.dispatchEvent(touchMove);

  assertFalse(addButton.classList.contains(test.mostVisited.CLASSES.REORDER));
  assertFalse(
      document.body.classList.contains(test.mostVisited.CLASSES.REORDERING));
};

/**
 * Tests if the held tile properly follows the cursor.
 */
test.mostVisited.testReorderFollowCursor = function() {
  // Set the window so that there's 10px padding around the grid.
  window.innerHeight = 40;
  window.innerWidth = 40;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    maxTilesPerRow: 2,
    maxTiles: 4,
    enableReorder: true
  };

  // Create a grid with max rows.
  let container = document.createElement('div');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGrid(container, params, 4);

  const dragStart = test.mostVisited.mockDragStart();
  const dragOver = new Event('dragover');
  const dragEnd = new Event('dragend');

  const tiles = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER);
  assertEquals(4, tiles.length);

  // Start the reorder flow on the center of the first tile.

  let tile = tiles[0];
  dragStart.pageX = 15;
  dragStart.pageY = 15;
  tile.firstChild.dispatchEvent(dragStart);
  // No style should be applied to the tile yet.
  assertEquals('', tile.firstChild.style.transform);

  // Move cursor 5px right and down. This should also move the tile 5px right
  // and down.
  dragOver.pageX = 20;
  dragOver.pageY = 20;
  let expectedTransform = 'translate(5px, 5px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Move cursor beyond the top right corner. This should move the tile to the
  // top right corner of the grid but not beyond it.
  dragOver.pageX = 40;
  dragOver.pageY = 0;
  expectedTransform = 'translate(10px, 0px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Move cursor beyond the bottom left corner. This should move the tile to the
  // bottom left corner of the grid but not beyond it.
  dragOver.pageX = 0;
  dragOver.pageY = 40;
  expectedTransform = 'translate(0px, 10px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);

  // Start the reorder flow on the center of the last tile.

  tile = tiles[3];
  dragStart.pageX = 25;
  dragStart.pageY = 25;
  tile.firstChild.dispatchEvent(dragStart);
  // No style should be applied to the tile yet.
  assertEquals('', tile.firstChild.style.transform);

  // Move cursor 5px left and up. This should also move the tile 5px left and
  // up.
  dragOver.pageX = 20;
  dragOver.pageY = 20;
  expectedTransform = 'translate(-5px, -5px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Move cursor beyond the bottom left corner. This should move the tile to the
  // bottom left corner of the grid but not beyond it.
  dragOver.pageX = 0;
  dragOver.pageY = 40;
  expectedTransform = 'translate(-10px, 0px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Move cursor beyond the top right corner. This should move the tile to the
  // top right corner of the grid but not beyond it.
  dragOver.pageX = 40;
  dragOver.pageY = 0;
  expectedTransform = 'translate(0px, -10px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);
};

/**
 * Tests if the held tile properly follows the cursor in RTL.
 */
test.mostVisited.testReorderFollowCursorRtl = function() {
  document.documentElement.dir = 'rtl';  // Enable RTL.
  // Set the window so that there's 10px padding around the grid.
  window.innerHeight = 40;
  window.innerWidth = 40;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    maxTilesPerRow: 2,
    maxTiles: 4,
    enableReorder: true
  };

  // Create a grid with max rows.
  let container = document.createElement('div');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGrid(container, params, 4);

  const dragStart = test.mostVisited.mockDragStart();
  const dragOver = new Event('dragover');
  const dragEnd = new Event('dragend');

  const tiles = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER);
  assertEquals(4, tiles.length);

  // Start the reorder flow on the center of the first tile.

  let tile = tiles[0];
  dragStart.pageX = 25;
  dragStart.pageY = 5;
  tile.firstChild.dispatchEvent(dragStart);
  // No style should be applied to the tile yet.
  assertEquals('', tile.firstChild.style.transform);

  // Move cursor beyond the top left corner. This should move the tile to the
  // top left corner of the grid but not beyond it.
  dragOver.pageX = 0;
  dragOver.pageY = 0;
  expectedTransform = 'translate(-10px, 0px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Move cursor beyond the bottom right corner. This should move the tile to
  // the bottom right corner of the grid but not beyond it.
  dragOver.pageX = 40;
  dragOver.pageY = 40;
  expectedTransform = 'translate(0px, 10px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);

  // Start the reorder flow on the center of the last tile.

  tile = tiles[3];
  dragStart.pageX = 5;
  dragStart.pageY = 25;
  tile.firstChild.dispatchEvent(dragStart);
  // No style should be applied to the tile yet.
  assertEquals('', tile.firstChild.style.transform);

  // Move cursor beyond the top left corner. This should move the tile to the
  // top left corner of the grid but not beyond it.
  dragOver.pageX = 0;
  dragOver.pageY = 0;
  expectedTransform = 'translate(0px, -10px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Move cursor beyond the bottom right corner. This should move the tile to
  // the bottom right corner of the grid but not beyond it.
  dragOver.pageX = 40;
  dragOver.pageY = 40;
  expectedTransform = 'translate(10px, 0px)';
  document.dispatchEvent(dragOver);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);
};

/**
 * Tests if the held tile properly follows the cursor using touch.
 */
test.mostVisited.testReorderFollowCursorTouch = function() {
  // Set the window so that there's 10px padding around the grid.
  window.innerHeight = 40;
  window.innerWidth = 40;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    maxTilesPerRow: 2,
    maxTiles: 4,
    enableReorder: true
  };

  // Create a grid with max rows.
  let container = document.createElement('div');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGrid(container, params, 4);

  const touchStart = new Event('touchstart');
  const touchMove = new Event('touchmove');
  const touchEnd = new Event('touchend');

  const tiles = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER);
  assertEquals(4, tiles.length);

  // Start the reorder flow on the center of the first tile.

  let tile = tiles[0];
  touchStart.changedTouches = [{pageX: 15, pageY: 15}];
  touchMove.changedTouches = [{pageX: 0, pageY: 0}];  // Point to some spot.
  tile.firstChild.dispatchEvent(touchStart);
  tile.firstChild.dispatchEvent(touchMove);
  // No style should be applied to the tile yet.
  assertEquals('', tile.firstChild.style.transform);

  // Move finger 5px right and down. This should also move the tile 5px right
  // and down.
  touchMove.changedTouches = [{pageX: 20, pageY: 20}];
  let expectedTransform = 'translate(5px, 5px)';
  // The first 'touchmove' event only starts the reorder flow, so a subsequent
  // move event is required.
  tile.firstChild.dispatchEvent(touchMove);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Move finger beyond the top right corner. This should move the tile to the
  // top right corner of the grid but not beyond it.
  touchMove.changedTouches = [{pageX: 40, pageY: 0}];
  expectedTransform = 'translate(10px, 0px)';
  tile.firstChild.dispatchEvent(touchMove);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Move finger beyond the bottom left corner. This should move the tile to the
  // bottom left corner of the grid but not beyond it.
  touchMove.changedTouches = [{pageX: 0, pageY: 40}];
  expectedTransform = 'translate(0px, 10px)';
  tile.firstChild.dispatchEvent(touchMove);

  assertEquals(expectedTransform, tile.firstChild.style.transform);

  // Stop the reorder flow.
  tile.firstChild.dispatchEvent(touchEnd);
};

/**
 * Tests if the tiles are translated properly when reordering.
 */
test.mostVisited.testReorderInsert = function() {
  // Set the window so that there's 10px padding around the grid.
  window.innerHeight = 40;
  window.innerWidth = 50;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    maxTilesPerRow: 3,
    maxTiles: 6,
    enableReorder: true
  };
  // Override for testing.
  let testElementsFromPoint = [];
  document.elementsFromPoint = (x, y) => {
    return testElementsFromPoint;
  };

  // Create a grid with uneven rows.
  let container = document.createElement('div');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGrid(container, params, 5);

  const dragStart = test.mostVisited.mockDragStart();
  dragStart.pageX = 0;  // Point to some spot.
  dragStart.pageY = 0;
  const dragOver = new Event('dragover');
  dragOver.pageX = 0;  // Point to some spot.
  dragOver.pageY = 0;
  const dragEnd = new Event('dragend');

  const tiles = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER);

  // Start the reorder flow on the first tile.

  let tile = tiles[0];
  tile.firstChild.dispatchEvent(dragStart);

  // Move over the second tile. This should shift tiles as if the held tile
  // was inserted after.
  let expectedLayout = [
    '', 'translate(-10px, 0px)', 'translate(0px, 0px)', 'translate(0px, 0px)',
    'translate(0px, 0px)'
  ];
  testElementsFromPoint = [tiles[1]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Move over the last tile. This should shift tiles as if the held tile was
  // inserted after.
  expectedLayout = [
    '', 'translate(-10px, 0px)', 'translate(-10px, 0px)',
    'translate(15px, -10px)', 'translate(-10px, 0px)'
  ];
  testElementsFromPoint = [tiles[4]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Move over the first tile. This should shift tiles as if the held tile was
  // inserted before.
  expectedLayout = [
    '', 'translate(0px, 0px)', 'translate(0px, 0px)', 'translate(0px, 0px)',
    'translate(0px, 0px)'
  ];
  testElementsFromPoint = [tiles[0]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);
  // Check that the correct values were sent to the EmbeddedSearchAPI.
  assertEquals(0, test.mostVisited.reorderRid);
  assertEquals(0, test.mostVisited.reorderNewIndex);

  // Start the reorder flow on the fourth tile.

  tile = tiles[3];
  tile.firstChild.dispatchEvent(dragStart);

  // Move over the first tile. This should shift tiles as if the held tile was
  // inserted before.
  expectedLayout = [
    'translate(10px, 0px)', 'translate(10px, 0px)', 'translate(-15px, 10px)',
    '', 'translate(0px, 0px)'
  ];
  testElementsFromPoint = [tiles[0]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 3);

  // Move over the last tile. This should shift tiles as if the held tile was
  // inserted after.
  expectedLayout = [
    'translate(0px, 0px)', 'translate(0px, 0px)', 'translate(0px, 0px)', '',
    'translate(-10px, 0px)'
  ];
  testElementsFromPoint = [tiles[4]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 3);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);
  // Check that the correct values were sent to the EmbeddedSearchAPI.
  assertEquals(3, test.mostVisited.reorderRid);
  assertEquals(4, test.mostVisited.reorderNewIndex);
};

/**
 * Tests if the tiles are translated properly when reordering in RTL. The (x,y)
 * translations should be the same as LTR except with a negative x value.
 */
test.mostVisited.testReorderInsertRtl = function() {
  document.documentElement.dir = 'rtl';  // Enable RTL.
  // Set the window so that there's 10px padding around the grid.
  window.innerHeight = 40;
  window.innerWidth = 50;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    maxTilesPerRow: 3,
    maxTiles: 6,
    enableReorder: true
  };
  // Override for testing.
  let testElementsFromPoint = [];
  document.elementsFromPoint = (x, y) => {
    return testElementsFromPoint;
  };

  // Create a grid with uneven rows.
  let container = document.createElement('div');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGrid(container, params, 5);

  const dragStart = test.mostVisited.mockDragStart();
  dragStart.pageX = 0;  // Point to some spot.
  dragStart.pageY = 0;
  const dragOver = new Event('dragover');
  dragOver.pageX = 0;  // Point to some spot.
  dragOver.pageY = 0;
  const dragEnd = new Event('dragend');

  const tiles = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER);

  // Start the reorder flow on the first tile.

  let tile = tiles[0];
  tile.firstChild.dispatchEvent(dragStart);

  // Move over the second tile. This should shift tiles as if the held tile
  // was inserted after.
  let expectedLayout = [
    '', 'translate(10px, 0px)', 'translate(0px, 0px)', 'translate(0px, 0px)',
    'translate(0px, 0px)'
  ];
  testElementsFromPoint = [tiles[1]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Move over the last tile. This should shift tiles as if the held tile was
  // inserted after.
  expectedLayout = [
    '', 'translate(10px, 0px)', 'translate(10px, 0px)',
    'translate(-15px, -10px)', 'translate(10px, 0px)'
  ];
  testElementsFromPoint = [tiles[4]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Move over the first tile. This should shift tiles as if the held tile was
  // inserted before.
  expectedLayout = [
    '', 'translate(0px, 0px)', 'translate(0px, 0px)', 'translate(0px, 0px)',
    'translate(0px, 0px)'
  ];
  testElementsFromPoint = [tiles[0]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);
  // Check that the correct values were sent to the EmbeddedSearchAPI.
  assertEquals(0, test.mostVisited.reorderRid);
  assertEquals(0, test.mostVisited.reorderNewIndex);

  // Start the reorder flow on the fourth tile.

  tile = tiles[3];
  tile.firstChild.dispatchEvent(dragStart);

  // Move over the first tile. This should shift tiles as if the held tile was
  // inserted before.
  expectedLayout = [
    'translate(-10px, 0px)', 'translate(-10px, 0px)', 'translate(15px, 10px)',
    '', 'translate(0px, 0px)'
  ];
  testElementsFromPoint = [tiles[0]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 3);

  // Move over the last tile. This should shift tiles as if the held tile was
  // inserted after.
  expectedLayout = [
    'translate(0px, 0px)', 'translate(0px, 0px)', 'translate(0px, 0px)', '',
    'translate(10px, 0px)'
  ];
  testElementsFromPoint = [tiles[4]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 3);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);
  // Check that the correct values were sent to the EmbeddedSearchAPI.
  assertEquals(3, test.mostVisited.reorderRid);
  assertEquals(4, test.mostVisited.reorderNewIndex);
};

/**
 * Tests if the tiles are translated properly when reordering with an add
 * shortcut button present.
 */
test.mostVisited.testReorderInsertWithAddButton = function() {
  // Set the window so that there's 10px padding around the grid.
  window.innerHeight = 40;
  window.innerWidth = 50;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    maxTilesPerRow: 3,
    maxTiles: 6,
    enableReorder: true
  };
  // Override for testing.
  let testElementsFromPoint = [];
  document.elementsFromPoint = (x, y) => {
    return testElementsFromPoint;
  };

  // Create a grid with uneven rows.
  let container = document.createElement('div');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGridWithAdd(container, params, 5);

  const dragStart = test.mostVisited.mockDragStart();
  dragStart.pageX = 0;  // Point to some spot.
  dragStart.pageY = 0;
  const dragOver = new Event('dragover');
  dragOver.pageX = 0;  // Point to some spot.
  dragOver.pageY = 0;
  const dragEnd = new Event('dragend');

  const tiles = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER);

  // Start the reorder flow on the second tile.

  let tile = tiles[1];
  tile.firstChild.dispatchEvent(dragStart);

  // Move over the first tile. This should shift tiles as if the held tile was
  // inserted before.
  let expectedLayout = [
    'translate(10px, 0px)', '', 'translate(0px, 0px)', 'translate(0px, 0px)', ''
  ];
  testElementsFromPoint = [tiles[0]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 1);

  // Move over the last tile (the add shortcut button). This should not shift
  // the tiles.
  testElementsFromPoint = [tiles[4]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 1);

  // Move over the second to last tile. This should shift tiles as if the held
  // tile was inserted after.
  expectedLayout = [
    'translate(0px, 0px)', '', 'translate(-10px, 0px)',
    'translate(15px, -10px)', ''
  ];
  testElementsFromPoint = [tiles[3]];
  document.dispatchEvent(dragOver);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 1);

  // Stop the reorder flow.
  document.dispatchEvent(dragEnd);
  // Check that the correct values were sent to the EmbeddedSearchAPI.
  assertEquals(1, test.mostVisited.reorderRid);
  assertEquals(3, test.mostVisited.reorderNewIndex);
};

/**
 * Tests if the tiles are translated properly when reordering using touch.
 */
test.mostVisited.testReorderInsertTouch = function() {
  // Set the window so that there's 10px padding around the grid.
  window.innerHeight = 40;
  window.innerWidth = 50;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    maxTilesPerRow: 3,
    maxTiles: 6,
    enableReorder: true
  };
  // Override for testing.
  let testElementsFromPoint = [];
  document.elementsFromPoint = (x, y) => {
    return testElementsFromPoint;
  };

  // Create a grid with uneven rows.
  let container = document.createElement('div');
  container.classList.add('test');
  $(test.mostVisited.MOST_VISITED).appendChild(container);
  test.mostVisited.initGrid(container, params, 5);
  $('most-visited').appendChild(container);

  const touchStart = new Event('touchstart');
  const touchMove = new Event('touchmove');
  const touchEnd = new Event('touchend');

  const tiles = document.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE_CONTAINER);
  assertEquals(5, tiles.length);

  // Start the reorder flow on the center of the first tile.

  let tile = tiles[0];
  touchStart.changedTouches = [{pageX: 15, pageY: 15}];
  touchMove.changedTouches = [{pageX: 0, pageY: 0}];  // Point to some spot.
  tile.firstChild.dispatchEvent(touchStart);
  tile.firstChild.dispatchEvent(touchMove);

  // Move over the second tile. This should shift tiles as if the held tile
  // was inserted after.
  let expectedLayout = [
    '', 'translate(-10px, 0px)', 'translate(0px, 0px)', 'translate(0px, 0px)',
    'translate(0px, 0px)'
  ];
  testElementsFromPoint = [tiles[1]];
  tile.firstChild.dispatchEvent(touchMove);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Move over the first tile. This should shift tiles as if the held tile was
  // inserted before.
  expectedLayout = [
    '', 'translate(0px, 0px)', 'translate(0px, 0px)', 'translate(0px, 0px)',
    'translate(0px, 0px)'
  ];
  testElementsFromPoint = [tiles[0]];
  tile.firstChild.dispatchEvent(touchMove);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Move over the last tile. This should shift tiles as if the held tile was
  // inserted after.
  expectedLayout = [
    '', 'translate(-10px, 0px)', 'translate(-10px, 0px)',
    'translate(15px, -10px)', 'translate(-10px, 0px)'
  ];
  testElementsFromPoint = [tiles[4]];
  tile.firstChild.dispatchEvent(touchMove);

  test.mostVisited.assertReorderInsert(container, expectedLayout, 0);

  // Stop the reorder flow.
  tile.firstChild.dispatchEvent(touchEnd);
  // Check that the correct values were sent to the EmbeddedSearchAPI.
  assertEquals(0, test.mostVisited.reorderRid);
  assertEquals(4, test.mostVisited.reorderNewIndex);
};

// ***************************** HELPER FUNCTIONS *****************************
// These are used by the tests above.

/**
 * Creates and initializes a MostVisited object.
 * @return {!Object} The MostVisited object.
 */
test.mostVisited.init = function() {
  const mostVisited = MostVisited();
  mostVisited.init();
  return mostVisited;
};

/**
 * Fills and initializes the tile grid.
 * @param {!Element} container The grid container.
 * @param {!Object} params Overrides for the default grid parameters.
 * @param {number} numTiles The number of tiles to fill the grid with.
 */
test.mostVisited.initGrid = function(container, params, numTiles) {
  assertTrue(!!test.mostVisited.grid);
  test.mostVisited.fillTiles(container, numTiles);
  test.mostVisited.grid.init(container, params);
};

/**
 * Fills and initializes the tile grid with an add button.
 * @param {!Element} container The grid container.
 * @param {!Object} params Overrides for the default grid parameters.
 * @param {number} numTiles The number of tiles to fill the grid with.
 */
test.mostVisited.initGridWithAdd = function(container, params, numTiles) {
  assertTrue(!!test.mostVisited.grid);
  test.mostVisited.fillTiles(container, numTiles - 1);
  // Append add button.
  test.mostVisited.appendTile(container, numTiles - 1, true);
  test.mostVisited.grid.init(container, params);
};

/**
 * Fills |container| with |numTiles| new grid tiles.
 * @param {!Element} container The grid container.
 * @param {number} numTiles Number of tiles to create.
 */
test.mostVisited.fillTiles = function(container, numTiles) {
  for (let i = 0; i < numTiles; i++) {
    test.mostVisited.appendTile(container, i, false);
  }
};

/**
 * Appends a grid tile to |container|.
 * @param {!Element} container The grid container.
 * @param {number} rid The tile's restricted id.
 * @param {boolean} isAddButton True if the tile is an add button.
 */
test.mostVisited.appendTile = function(container, rid, isAddButton) {
  const tile = document.createElement('div');
  const gridTile = test.mostVisited.grid.createGridTile(tile, rid, isAddButton);
  container.appendChild(gridTile);
};

/**
 * Returns a mock 'dragstart' event.
 * @return {Event}
 */
test.mostVisited.mockDragStart = function() {
  const dragStart = new Event('dragstart');
  dragStart.dataTransfer = {
    setData: (a, b) => {},
    setDragImage: (a, b, c) => {},
  };
  return dragStart;
};

/**
 * Assert that the tile layout matches |expectedLayout|.
 * @param {!Element} container The grid container.
 * @param {!Array<string>} expectedLayout The transform values for each tile.
 */
test.mostVisited.assertLayout = function(container, expectedLayout) {
  const tiles = container.children;
  assertEquals(expectedLayout.length, tiles.length);
  for (let i = 0; i < tiles.length; i++) {
    assertEquals(expectedLayout[i], tiles[i].style.transform);
  }
};

/**
 * Assert that all but the last |numHidden| tiles are visible.
 * @param {!Element} container The grid container.
 * @param {number} numHidden The number of hidden tiles.
 */
test.mostVisited.assertVisibility = function(container, numHidden) {
  const tiles = container.children;
  for (let i = 0; i < tiles.length; i++) {
    if (i < tiles.length - numHidden) {
      assertFalse(tiles[i].style.display === 'none');
    } else {
      assertTrue(tiles[i].style.display === 'none');
    }
  }
};

/**
 * Assert that the tile layout matches |expectedLayout| after the reorder
 * insertion.
 * @param {!Element} container The grid container.
 * @param {!Array<string>} expectedLayout The transform values for each tile.
 * @param {number} heldIndex Index of the held tile.
 */
test.mostVisited.assertReorderInsert = function(
    container, expectedLayout, heldIndex) {
  const tiles = container.getElementsByClassName(
      test.mostVisited.CLASSES.GRID_TILE);
  assertEquals(expectedLayout.length, tiles.length);
  for (let i = 0; i < tiles.length; i++) {
    if (i === heldIndex) {
      continue;
    }
    assertEquals(expectedLayout[i], tiles[i].style.transform);
  }
};
