// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the ListPropertyUpdateBehavior.  */

// #import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
// #import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('ListPropertyUpdateBehavior', function() {
  /**
   * A list property update behavior test element created before each test.
   * @type {ListPropertyUpdateBehaviorTestElement}
   */
  let testElement;

  suiteSetup(function() {
    /** A test element that implements the ListPropertyUpdateBehavior. */
    Polymer({
      is: 'list-property-update-behavior-test-element',

      behaviors: [ListPropertyUpdateBehavior],

      properties: {
        /**
         * A test array containing objects with Array properties. The elements
         * in the array represent an object that maps a list of |words| to the
         * |letter| that they begin with.
         * @type {!Array<{letter: !string, words: !Array<string>}>}
         */
        complexArray: {
          type: Array,
        },

        /**
         * A test array containing objects with numerical |id|s.
         * @type {!Array<{id: !number}>}
         */
        simpleArray: {
          type: Array,
        },
      },

      /** @override */
      created: function() {
        this.resetSimpleArray();
        this.resetComplexArray();
      },

      resetComplexArray() {
        this.complexArray = [
          {letter: 'a', words: ['adventure', 'apple']},
          {letter: 'b', words: ['banana', 'bee', 'bottle']},
          {letter: 'c', words: ['car']},
        ];
      },

      resetSimpleArray() {
        this.simpleArray = [{id: 1}, {id: 2}, {id: 3}];
      },

      /**
       * Updates the |complexArray| with |newArray| using the
       * ListPropertyUpdateBehavior.updateList() method. This method will
       * iterate through the elements of |complexArray| to check if their
       * |words| property array need to be updated if |complexArray| did not
       * have any changes.
       * @param {!Array{letter: !string, words: !Array<string>}>} newArray The
       *     array update |complexArray| with.
       * @returns {{topArrayChanged: boolean, wordsArrayChanged: boolean}} An
       *     object that has a |topArrayChanged| property set to true if
       *     notifySplices() was called for the 'complexArray' property path and
       *     a |wordsArrayChanged| property set to true if notifySplices() was
       *     called for the |words| property on an item of |complexArray|.
       */
      updateComplexArray(newArray) {
        if (this.updateList(
                'complexArray', x => x.letter, newArray,
                true /* uidBasedUpdate */)) {
          return {topArrayChanged: true, wordsArrayChanged: false};
        }

        // At this point, |complexArray| and |newArray| should have the same
        // elements.
        let wordsSplicesNotified = false;
        assertEquals(this.complexArray.length, newArray.length);
        this.complexArray.forEach((item, i) => {
          assertEquals(item.letter, newArray[i].letter);
          const propertyPath = 'complexArray.' + i + '.words';
          const newWordsArray = newArray[i].words;

          if (this.updateList(propertyPath, x => x, newWordsArray)) {
            wordsSplicesNotified = true;
          }
        });

        return {
          topArrayChanged: false,
          wordsArrayChanged: wordsSplicesNotified,
        };
      },

      /**
       * Updates the |simpleArray| with |newArray| using the
       * ListPropertyUpdateBehavior.updateList() method.
       * @param {!Array{id: !number}>} newArray The array to update
       *     |simpleArray| with.
       * @returns {boolean} True if the update called notifySplices() for
       *     |simpleArray|.
       */
      updateSimpleArray(newArray) {
        return this.updateList('simpleArray', x => x.id, newArray);
      },
    });
  });

  // Initialize a list-property-update-behavior-test-element before each test.
  setup(function() {
    PolymerTest.clearBody();
    testElement =
        document.createElement('list-property-update-behavior-test-element');
    document.body.appendChild(testElement);
  });

  function assertSimpleArrayEquals(array, expectedArray) {
    assertEquals(array.length, expectedArray.length);
    array.forEach((item, i) => {
      assertEquals(item.id, expectedArray[i].id);
    });
  }

  function assertComplexArrayEquals(array, expectedArray) {
    assertEquals(array.length, expectedArray.length);
    array.forEach((item, i) => {
      assertEquals(item.letter, expectedArray[i].letter);
      assertEquals(item.words.length, expectedArray[i].words.length);

      item.words.forEach((word, j) => {
        assertEquals(word, expectedArray[i].words[j]);
      });
    });
  }

  test(
      'notifySplices() is not called when a simple array has not been changed',
      function() {
        let unchangedSimpleArray = testElement.simpleArray.slice();
        assertFalse(testElement.updateSimpleArray(unchangedSimpleArray));
      });

  test(
      'notifySplices() is not called when a complex array has not been changed',
      function() {
        let unchangedComplexArray = testElement.complexArray.slice();
        let result = testElement.updateComplexArray(unchangedComplexArray);
        assertFalse(result.topArrayChanged);
        assertFalse(result.wordsArrayChanged);
      });

  test(
      'notifySplices() is called when a simple array has been changed',
      function() {
        // Ensure that the array is updated when an element is removed from the
        // end.
        let elementRemoved = testElement.simpleArray.slice(0, 2);

        assertTrue(testElement.updateSimpleArray(elementRemoved));
        assertSimpleArrayEquals(testElement.simpleArray, elementRemoved);

        // Ensure that the array is updated when an element is removed from the
        // beginning.
        testElement.resetSimpleArray();
        elementRemoved = testElement.simpleArray.slice(1);

        assertTrue(testElement.updateSimpleArray(elementRemoved));
        assertSimpleArrayEquals(testElement.simpleArray, elementRemoved);

        // Ensure that the array is updated when an element is added to the end.
        testElement.resetSimpleArray();
        let elementAdded = testElement.simpleArray.slice();
        elementAdded.push({id: 4});

        assertTrue(testElement.updateSimpleArray(elementAdded));
        assertSimpleArrayEquals(testElement.simpleArray, elementAdded);

        // Ensure that the array is updated when an element is added to the
        // beginning.
        testElement.resetSimpleArray();
        elementAdded = [{id: 0}];
        elementAdded.push(...testElement.simpleArray.slice());

        assertTrue(testElement.updateSimpleArray(elementAdded));
        assertSimpleArrayEquals(testElement.simpleArray, elementAdded);

        // Ensure that the array is updated when the entire array is different.
        testElement.resetSimpleArray();
        let newArray = [{id: 10}, {id: 11}, {id: 12}, {id: 13}];

        assertTrue(testElement.updateSimpleArray(newArray));
        assertSimpleArrayEquals(testElement.simpleArray, newArray);
      });

  test(
      'notifySplices() is called when the top array of a complex array has ' +
          'been changed',
      function() {
        // Ensure that the array is updated when an element is removed from the
        // end.
        let elementRemoved = testElement.complexArray.slice(0, 2);
        let result = testElement.updateComplexArray(elementRemoved);

        assertTrue(result.topArrayChanged);
        assertFalse(result.wordsArrayChanged);
        assertComplexArrayEquals(testElement.complexArray, elementRemoved);

        // Ensure that the array is updated when an element is removed from the
        // beginning.
        testElement.resetComplexArray();
        elementRemoved = testElement.complexArray.slice(1);
        result = testElement.updateComplexArray(elementRemoved);

        assertTrue(result.topArrayChanged);
        assertFalse(result.wordsArrayChanged);
        assertComplexArrayEquals(testElement.complexArray, elementRemoved);

        // Ensure that the array is updated when an element is added to the end.
        testElement.resetComplexArray();
        let elementAdded = testElement.complexArray.slice();
        elementAdded.push({letter: 'd', words: ['door', 'dice']});
        result = testElement.updateComplexArray(elementAdded);

        assertTrue(result.topArrayChanged);
        assertFalse(result.wordsArrayChanged);
        assertComplexArrayEquals(testElement.complexArray, elementAdded);

        // Ensure that the array is updated when an element is added to the
        // beginning.
        testElement.resetComplexArray();
        elementAdded = [{letter: 'A', words: ['Alphabet']}];
        elementAdded.push(...testElement.complexArray.slice());
        result = testElement.updateComplexArray(elementAdded);

        assertTrue(result.topArrayChanged);
        assertFalse(result.wordsArrayChanged);
        assertComplexArrayEquals(testElement.complexArray, elementAdded);

        // Ensure that the array is updated when the entire array is different.
        testElement.resetComplexArray();
        let newArray = [
          {letter: 'w', words: ['water', 'woods']},
          {letter: 'x', words: ['xylophone']}, {letter: 'y', words: ['yo-yo']},
          {letter: 'z', words: ['zebra', 'zephyr']}
        ];
        result = testElement.updateComplexArray(newArray);

        assertTrue(result.topArrayChanged);
        assertFalse(result.wordsArrayChanged);
        assertComplexArrayEquals(testElement.complexArray, newArray);
      });

  test(
      'notifySplices() is called when an array property of a complex array ' +
          'element is changed',
      function() {
        // Ensure that the |words| property of a |complexArray| element is
        // updated properly.
        let newArray = JSON.parse(JSON.stringify(testElement.complexArray));
        newArray[1].words = newArray[1].words.slice(0, 2);
        let result = testElement.updateComplexArray(newArray);

        assertFalse(result.topArrayChanged);
        assertTrue(result.wordsArrayChanged);
        assertComplexArrayEquals(testElement.complexArray, newArray);

        // Ensure that the array is properly updated when the |words| array of
        // multiple elements are modified.
        testElement.resetComplexArray();
        newArray = JSON.parse(JSON.stringify(testElement.complexArray));
        newArray[0].words.push('apricot');
        newArray[1].words = newArray[1].words.slice(1);
        newArray[2].words.push('circus', 'citrus', 'carrot');
        result = testElement.updateComplexArray(newArray);

        assertFalse(result.topArrayChanged);
        assertTrue(result.wordsArrayChanged);
        assertComplexArrayEquals(testElement.complexArray, newArray);
      });

  test('first item with same uid modified', () => {
    const newArray = JSON.parse(JSON.stringify(testElement.complexArray));
    assertTrue(newArray[0].words.length > 0);
    assertNotEquals('apricot', newArray[0].words[0]);
    newArray[0].words = ['apricot'];
    assertTrue(testElement.updateList('complexArray', x => x.letter, newArray));
    assertDeepEquals(['apricot'], testElement.complexArray[0].words);
  });

  test('first item modified with same uid and last item removed', () => {
    const newArray = JSON.parse(JSON.stringify(testElement.complexArray));
    assertTrue(newArray[0].words.length > 0);
    assertNotEquals('apricot', newArray[0].words[0]);
    newArray[0].words = ['apricot'];
    assertTrue(newArray.length > 1);
    newArray.pop();
    assertTrue(testElement.updateList('complexArray', x => x.letter, newArray));
    assertDeepEquals(['apricot'], testElement.complexArray[0].words);
  });
});
