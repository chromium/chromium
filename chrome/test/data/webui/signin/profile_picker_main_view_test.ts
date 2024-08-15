// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/profile_picker.js';

import type {ProfileCardElement, ProfilePickerMainViewElement, ProfileState} from 'chrome://profile-picker/profile_picker.js';
import {loadTimeData, ManageProfilesBrowserProxyImpl, NavigationMixin, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

class NavigationElement extends NavigationMixin(CrLitElement) {
  static get is() {
    return 'navigation-element';
  }

  changeCalled: boolean = false;
  route: string = '';

  override firstUpdated() {
    this.reset();
  }

  override onRouteChange(route: Routes, _step: string) {
    this.changeCalled = true;
    this.route = route;
  }

  reset() {
    this.changeCalled = false;
    this.route = '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'navigation-element': NavigationElement;
  }
}

customElements.define(NavigationElement.is, NavigationElement);

/**
 * @param n Indicates the desired number of profiles.
 */
function generateProfilesList(n: number): ProfileState[] {
  return Array(n)
      .fill(0)
      .map((_x, i) => i % 2 === 0)
      .map((sync, i) => ({
             profilePath: `profilePath${i}`,
             localProfileName: `profile${i}`,
             isSyncing: sync,
             needsSignin: false,
             gaiaName: sync ? `User${i}` : '',
             userName: sync ? `User${i}@gmail.com` : '',
             avatarIcon: `AvatarUrl-${i}`,
             avatarBadge: i % 4 === 0 ? `cr:domain` : ``,
             // <if expr="chromeos_lacros">
             isPrimaryLacrosProfile: false,
             // </if>
           }));
}

suite('ProfilePickerMainViewTest', function() {
  let mainViewElement: ProfilePickerMainViewElement;
  let browserProxy: TestManageProfilesBrowserProxy;
  let navigationElement: NavigationElement;

  function resetTest() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    navigationElement = document.createElement('navigation-element');
    document.body.appendChild(navigationElement);
    mainViewElement = document.createElement('profile-picker-main-view');
    document.body.appendChild(mainViewElement);
  }

  function resetPolicies() {
    // This is necessary as |loadTimeData| state leaks between tests.
    // Any load time data manipulated by the tests needs to be reset here.
    loadTimeData.overrideValues({
      isGuestModeEnabled: true,
      isProfileCreationAllowed: true,
      isAskOnStartupAllowed: true,
      profilesReorderingEnabled: true,
    });
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    resetPolicies();
    resetTest();
  });

  async function simulateProfilesListChanged(profiles: ProfileState[]) {
    webUIListenerCallback('profiles-list-changed', [...profiles]);

    // Await for the profiles to be rendered before proceeding.
    await microtasksFinished();
  }

  async function simulateProfileRemoved(profilePath: string) {
    webUIListenerCallback('profile-removed', profilePath);

    // Await for the profiles to be rendered before proceeding.
    await microtasksFinished();
  }

  async function verifyProfileCard(
      expectedProfiles: ProfileState[],
      profiles: NodeListOf<ProfileCardElement>) {
    assertEquals(expectedProfiles.length, profiles.length);
    for (let i = 0; i < expectedProfiles.length; i++) {
      const profile = profiles[i]!;
      const expectedProfile = expectedProfiles[i]!;
      assertTrue(!!profile.shadowRoot!.querySelector('profile-card-menu'));
      profile.shadowRoot!.querySelector('cr-button')!.click();
      await browserProxy.whenCalled('launchSelectedProfile');
      assertEquals(
          profile.shadowRoot!
              .querySelector<HTMLElement>('#forceSigninContainer')!.hidden,
          !expectedProfile.needsSignin);

      const gaiaName = profile.$.gaiaName;
      assertEquals(gaiaName.hidden, expectedProfile.needsSignin);
      assertEquals(gaiaName.innerText.trim(), expectedProfile.gaiaName);

      assertEquals(profile.$.nameInput.value, expectedProfile.localProfileName);
      assertEquals(
          profile.shadowRoot!.querySelector<HTMLElement>(
                                 '#iconContainer')!.hidden,
          !expectedProfile.avatarBadge);
      assertEquals(
          (profile.shadowRoot!
               .querySelector<HTMLImageElement>('.profile-avatar')!.src)
              .split('/')
              .pop(),
          expectedProfile.avatarIcon);
    }
  }

  test('MainViewWithDefaultPolicies', async function() {
    assertTrue(navigationElement.changeCalled);
    assertEquals(navigationElement.route, Routes.MAIN);
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(6);
    await simulateProfilesListChanged(profiles);
    // Profiles list defined.
    assertTrue(!mainViewElement.$.profilesContainer.hidden);
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    assertTrue(mainViewElement.$.askOnStartup.checked);
    // Verify profile card.
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // Browse as guest.
    assertTrue(!!mainViewElement.$.browseAsGuestButton);
    mainViewElement.$.browseAsGuestButton.click();
    await browserProxy.whenCalled('launchGuestProfile');
    // Ask when chrome opens.
    mainViewElement.$.askOnStartup.click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(!mainViewElement.$.askOnStartup.checked);
    // Update profile data.
    profiles[1] = profiles[4]!;
    await simulateProfilesListChanged(profiles);
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // Profiles update on remove.
    await simulateProfileRemoved(profiles[3]!.profilePath);
    profiles.splice(3, 1);
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
  });

  test('EditLocalProfileName', async function() {
    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(1);
    await simulateProfilesListChanged(profiles);
    const localProfileName =
        mainViewElement.shadowRoot!.querySelector('profile-card')!.$.nameInput;
    assertEquals(localProfileName.value, profiles[0]!.localProfileName);

    // Set to valid profile name.
    localProfileName.value = 'Alice';
    localProfileName.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const args = await browserProxy.whenCalled('setProfileName');
    assertEquals(args[0], profiles[0]!.profilePath);
    assertEquals(args[1], 'Alice');
    assertEquals(localProfileName.value, 'Alice');

    // Set to invalid profile name
    localProfileName.value = '';
    await localProfileName.updateComplete;
    assertTrue(localProfileName.invalid);
  });

  test('GuestModeDisabled', async function() {
    loadTimeData.overrideValues({
      isGuestModeEnabled: false,
    });
    resetTest();
    assertEquals(mainViewElement.$.browseAsGuestButton.style.display, 'none');
    await browserProxy.whenCalled('initializeMainView');
    await simulateProfilesListChanged(generateProfilesList(2));
    assertEquals(mainViewElement.$.browseAsGuestButton.style.display, 'none');
  });

  test('ProfileCreationNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    resetTest();
    const addProfile =
        mainViewElement.shadowRoot!.querySelector<HTMLElement>('#addProfile')!;
    assertEquals(addProfile.style.display, 'none');
    await browserProxy.whenCalled('initializeMainView');
    await simulateProfilesListChanged(generateProfilesList(2));
    navigationElement.reset();
    assertEquals(addProfile.style.display, 'none');
    addProfile.click();
    await microtasksFinished();
    assertTrue(!navigationElement.changeCalled);
  });

  test('AskOnStartupSingleToMultipleProfiles', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    let profiles = generateProfilesList(1);
    await simulateProfilesListChanged(profiles);
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // The checkbox 'Ask when chrome opens' should only be visible to
    // multi-profile users.
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    // Add a second profile.
    profiles = generateProfilesList(2);
    await simulateProfilesListChanged(profiles);
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    assertTrue(mainViewElement.$.askOnStartup.checked);
    mainViewElement.$.askOnStartup.click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(!mainViewElement.$.askOnStartup.checked);
  });

  test('AskOnStartupMultipleToSingleProfile', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(2);
    await simulateProfilesListChanged(profiles);
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    // Remove profile.
    await simulateProfileRemoved(profiles[0]!.profilePath);
    await verifyProfileCard(
        [profiles[1]!],
        mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(mainViewElement.$.askOnStartup.hidden);
  });

  test('AskOnStartupMulipleProfiles', async function() {
    // Disable AskOnStartup
    loadTimeData.overrideValues({isAskOnStartupAllowed: false});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(2);
    await simulateProfilesListChanged(profiles);
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));

    // Checkbox hidden even if there are multiple profiles.
    assertTrue(mainViewElement.$.askOnStartup.hidden);
  });

  test('ForceSigninIsEnabled', async function() {
    loadTimeData.overrideValues({isForceSigninEnabled: true});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(2);
    profiles[0]!.needsSignin = true;
    await simulateProfilesListChanged(profiles);
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
  });
});

suite('ProfilePickerProfilesReorderingTest', function() {
  let mainViewElement: ProfilePickerMainViewElement;
  let browserProxy: TestManageProfilesBrowserProxy;

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    loadTimeData.overrideValues({
      profilesReorderingEnabled: true,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mainViewElement = document.createElement('profile-picker-main-view');
    document.body.appendChild(mainViewElement);
  });

  // Sets up the profile picker with the reorder functionality and creates
  // profiles.
  async function setupProfileReorderingTest(numberOfProfiles: number) {
    // Activates the profile reordering feature.
    loadTimeData.overrideValues({profilesReorderingEnabled: true});

    // Remove transition duration to avoid waiting during tests.
    mainViewElement.setDraggingTransitionDurationForTesting(0);

    // Create the profiles and push them to the main profile picker view.
    const profiles = generateProfilesList(numberOfProfiles);
    webUIListenerCallback('profiles-list-changed', [...profiles]);

    // Await for the profiles to be rendered before proceeding.
    await microtasksFinished();
  }

  // This function makes sure that the test data is valid and consistent.
  function checkTestData(
      expectedInitialProfileOrder: string[], dragEventCycles: Array<{
        dragIndex: number,
        dragEnterEvents:
            Array<{index: number, expectedResultIndices: number[]}>,
        expectedEndProfileOrder: string[],
      }>) {
    const profileSet = new Set();
    expectedInitialProfileOrder.forEach((profileName) => {
      assertFalse(
          profileSet.has(profileName),
          'Test setup error: Profile name is repeated in the initial profiles');
      profileSet.add(profileName);
    });

    const numberOfProfiles = expectedInitialProfileOrder.length;
    dragEventCycles.forEach((cycle) => {
      assertTrue(
          0 <= cycle.dragIndex && cycle.dragIndex < numberOfProfiles,
          'Test setup error: Drag index is out of bounds.');

      cycle.dragEnterEvents.forEach(event => {
        assertTrue(
            0 <= event.index && event.index < numberOfProfiles,
            'Test setup error:  Event index is out of bounds');
        const indicesSet = new Set();
        event.expectedResultIndices.forEach(resultIndex => {
          assertTrue(
              0 <= resultIndex && resultIndex < numberOfProfiles,
              'Test setup error:  Expected index result is out of bounds');
          assertFalse(
              indicesSet.has(resultIndex),
              'Test setup error: Expected indices should\'nt have a repeated index');
          indicesSet.add(resultIndex);
        });

        assertEquals(
            numberOfProfiles, event.expectedResultIndices.length,
            'Test setup error: `expectedResultIndices` length should match the profile count');
      });
    });
  }

  // This function tests the current DOMCRect of the elements in `cards` with
  // their expected positions in `expectedIndices` vs their initial positions
  // stored in `initialRects` with their initial position indices (that are
  // simply ordered).
  // This check works because we expect profile cards to take the spot of other
  // cards when they move due to drag events.
  function assertProfilesPositions(
      cards: ProfileCardElement[], draggingIndex: number|null,
      initialRects: DOMRect[], expectedIndices: number[],
      errorMessage: string = '') {
    for (let i = 0; i < expectedIndices.length; ++i) {
      const expectedIndex = expectedIndices[i]!;
      // Do not compare the dragging profile element position, as it does not
      // move, it is only hidden.
      if (draggingIndex != null && draggingIndex === expectedIndex) {
        assertTrue(cards[draggingIndex]!.classList.contains('dragging'));
        continue;
      }

      // Initial indices are ordered, so we can use the loop index as the
      // basis.
      assertDeepEquals(
          cards[expectedIndex]!.getBoundingClientRect(), initialRects[i],
          errorMessage);
    }
  }

  // This function compares the profile names in the ProfileState array versus
  // the given expected list.
  function assertProfileNamesOrder(
      profiles: ProfileState[], profileNames: string[],
      errorMessage: string = '') {
    assertEquals(profiles.length, profileNames.length);
    assertDeepEquals(
        profiles.map((profile) => {
          return profile.localProfileName;
        }),
        profileNames, errorMessage);
  }

  // This test function simulates drag event cycles.
  // It first creates multiple profiles based on `numberOfProfiles` and
  // initialize the profile picker main view.

  // In each cycle it performs some drag events:
  // - dragstart: with the given `dragIndex`.
  // - dragenter: multiple events through `dragEnterEvents`.
  // - dragend: with the given `dragIndex`.
  //
  // Multiple checks are done through the test to guarantee the state after each
  // drag event:
  //
  // The initial profile order list is first checked.
  // Then per drag cycle, we first make sure that the state at the start of the
  // cycle is coherent, it should either be the initial profile order if this is
  // the first cycle, or the ending expected profile order of the previous
  // cycle.
  // In each cycle the indices are reset and do not follow the profile state
  // positions, we use these indices to check the profile movements within one
  // cycle, where the indices will be swapped around.
  // And finally we check the `expectedEndProfileOrder` of profile names that is
  // resulted at the end of the cycle.
  //
  // Variables:
  // - `numberOfProfiles`: number of profiles created for the drag cycle.
  // - `expectedInitialProfileOrder`: initial order of the profile names.
  // - `dragEventCycles`: list of drag event cycles.
  //    - `dragIndex`: the index of the card being dragged in this cycle.
  //    - `dragEnterEvents`: the list of drag enters that will happen in the
  //    cycle.
  //       - `index`: the index of the card that will be entered, this will not
  //       necessarily match with the position of the card in the list
  //       (especially after multiple enters and resets), but rather the tile
  //       that has the index to be entered.
  //       - `expectedResultIndices`: the expected indices of profiles after the
  //    drag enter event.
  //    - `expectedEndProfileOrder`: expected profile name order at the end of
  //    the cycle.
  async function testProfileReorderingDragCycles(dragData: {
    expectedInitialProfileOrder: string[],
    dragEventCycles: Array<{
      dragIndex: number,
      dragEnterEvents: Array<{index: number, expectedResultIndices: number[]}>,
      expectedEndProfileOrder: string[],
    }>,
  }) {
    // Preliminary `dragData` checks before proceeding with the actual test.
    checkTestData(
        dragData.expectedInitialProfileOrder, dragData.dragEventCycles);

    const numberOfProfiles = dragData.expectedInitialProfileOrder.length;
    await setupProfileReorderingTest(numberOfProfiles);
    // Assert the initial profile order.
    assertProfileNamesOrder(
        mainViewElement.getProfileListForTesting(),
        dragData.expectedInitialProfileOrder, 'Initial order check');

    // ----------------- Start of the Drag Cycles -----------------

    for (let c = 0; c < dragData.dragEventCycles.length; ++c) {
      let expectedPreviousEndProfileOrder: string[];
      // For the first cycle, the order is the initial order.
      // For the rest of the cycles, it is the ending order of the previous
      // cycle.
      if (c === 0) {
        expectedPreviousEndProfileOrder = dragData.expectedInitialProfileOrder;
      } else {
        expectedPreviousEndProfileOrder =
            dragData.dragEventCycles[c - 1]!.expectedEndProfileOrder;
      }
      assertProfileNamesOrder(
          mainViewElement.getProfileListForTesting(),
          expectedPreviousEndProfileOrder, `Cycle ${c} initial order check`);

      const cards = Array.from(
          mainViewElement.shadowRoot!.querySelectorAll<ProfileCardElement>(
              'profile-card'));

      // Store the initial profile cards rects for later comparison.
      const initialRects =
          cards.map(card => card.getBoundingClientRect()) as DOMRect[];

      // // Equivalent to an array {0, 1, 2. ... , numberOfProfiles - 1}.
      const initialIndices = Array.from(Array(numberOfProfiles).keys());
      // Check that the initial positions of the profiles are aligned.
      assertProfilesPositions(
          cards, null, initialRects, initialIndices,
          `Cycle ${c}: initial indicies check.`);

      // ----------------- Start of the Drag Events -----------------

      const cycle = dragData.dragEventCycles[c]!;
      // - Perform the Drag Start.
      cards[cycle.dragIndex]!.dispatchEvent(new DragEvent('dragstart'));
      assertTrue(cards[cycle.dragIndex]!.classList.contains('dragging'));

      // - Perform the list of Drag Enter events with position checks.
      for (let e = 0; e < cycle.dragEnterEvents.length; ++e) {
        const event = cycle.dragEnterEvents[e]!;
        // Perform the drag enter event.
        cards[event.index]!.dispatchEvent(new DragEvent('dragenter'));
        // Check that the positions of the cards are as expected after each
        // enter events.
        assertProfilesPositions(
            cards, cycle.dragIndex, initialRects, event.expectedResultIndices,
            `Cycle ${c}, Enter ${e}: profile card positions check.`);
      }

      // - Perform the Drag End.
      cards[cycle.dragIndex]!.dispatchEvent(new DragEvent('dragend'));
      assertFalse(cards[cycle.dragIndex]!.classList.contains('dragging'));

      assertProfileNamesOrder(
          mainViewElement.getProfileListForTesting(),
          cycle.expectedEndProfileOrder, `Cycle ${c} end order check`);
    }
  }

  // A helper function that test a single cycle of drag events.
  // It constructs the inputs for the main testing function.
  async function testProfileReorderingDragCycle(dragData: {
    expectedInitialProfileOrder: string[],
    dragIndex: number,
    dragEnterEvents: Array<{index: number, expectedResultIndices: number[]}>,
    expectedEndProfileOrder: string[],
  }) {
    // Adapts the function to 1 cycle.
    testProfileReorderingDragCycles({
      expectedInitialProfileOrder: dragData.expectedInitialProfileOrder,
      dragEventCycles: [{
        dragIndex: dragData.dragIndex,
        dragEnterEvents: dragData.dragEnterEvents,
        expectedEndProfileOrder: dragData.expectedEndProfileOrder,
      }],
    });
  }

  test('ProfileReorder_DragStartEndNoEnter', async function() {
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragIndex: 1,
      dragEnterEvents: [],
      expectedEndProfileOrder: ['profile0', 'profile1', 'profile2'],
    });
  });

  // This test simulates the dragged card to generate a 'dragenter' event on
  // itself (the hidden dragging card). This should have no effect on the order.
  test('ProfileReorder_DragEnterOnDraggedCardHasNoEffect', async function() {
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragIndex: 1,
      dragEnterEvents: [{index: 1, expectedResultIndices: [0, 1, 2]}],
      expectedEndProfileOrder: ['profile0', 'profile1', 'profile2'],
    });
  });

  test('ProfileReorder_DragEnterOnNextCard', async function() {
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragIndex: 1,
      dragEnterEvents: [{index: 2, expectedResultIndices: [0, 2, 1]}],
      expectedEndProfileOrder: ['profile0', 'profile2', 'profile1'],
    });
  });

  // In this test, we make sure that even in the middle of a drag event cycle
  // (after performing at least one real shift), we still do no react to
  // entering the initial dragging tile.
  test('ProfileReorder_DragEnterItselfAfterShifts', async function() {
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragIndex: 1,
      dragEnterEvents: [
        {index: 2, expectedResultIndices: [0, 2, 1]},
        {index: 1, expectedResultIndices: [0, 2, 1]},
      ],
      expectedEndProfileOrder: ['profile0', 'profile2', 'profile1'],
    });
  });

  test('ProfileReorder_DragEnterOnFurtherCard', async function() {
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragIndex: 0,
      dragEnterEvents: [{index: 2, expectedResultIndices: [1, 2, 0]}],
      expectedEndProfileOrder: ['profile1', 'profile2', 'profile0'],
    });
  });

  test('ProfileReorder_DragMultipleEnters', async function() {
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder:
          ['profile0', 'profile1', 'profile2', 'profile3'],
      dragIndex: 3,
      dragEnterEvents: [
        {index: 2, expectedResultIndices: [0, 1, 3, 2]},
        {index: 1, expectedResultIndices: [0, 3, 1, 2]},
        {index: 0, expectedResultIndices: [3, 0, 1, 2]},
      ],
      expectedEndProfileOrder: ['profile3', 'profile0', 'profile1', 'profile2'],
    });
  });

  // This test makes sure that if any draggable object (not a profile) triggers
  // a `dragenter` event for a profile-card the reordering is not triggered and
  // the profile order is not affected.
  test('ProfileReorder_DragEnterOutOfDragCycleHasNoEffect', async function() {
    await setupProfileReorderingTest(3);

    const cards = Array.from(
        mainViewElement.shadowRoot!.querySelectorAll<ProfileCardElement>(
            'profile-card'));

    // Store the initial profile cards rects for later comparison.
    const initialRects =
        cards.map(card => card.getBoundingClientRect()) as DOMRect[];
    const initiIndices = [0, 1, 2];
    assertProfilesPositions(
        cards, null, initialRects, initiIndices, 'Initial indicies check.');

    // Simulate a dragenter event without having a prior dragstart that started
    // the drag cycle event.
    cards[0]!.dispatchEvent(new DragEvent('dragenter'));

    // Same assertion as the drag event should have no effect, or cause no
    // crash.
    assertProfilesPositions(
        cards, null, initialRects, initiIndices,
        'Expected same value as initial check');
  });

  test('ProfileReorder_SingleEnterWithReset', async function() {
    // Last enter event enters the shifted card.
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragIndex: 1,
      dragEnterEvents: [
        {index: 2, expectedResultIndices: [0, 2, 1]},
        {index: 2, expectedResultIndices: [0, 1, 2]},
      ],
      expectedEndProfileOrder: ['profile0', 'profile1', 'profile2'],
    });
  });

  test('ProfileReorder_MultipleEntersWithResets', async function() {
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder:
          ['profile0', 'profile1', 'profile2', 'profile3'],
      dragIndex: 0,
      dragEnterEvents: [
        {index: 1, expectedResultIndices: [1, 0, 2, 3]},
        {index: 2, expectedResultIndices: [1, 2, 0, 3]},
        {index: 3, expectedResultIndices: [1, 2, 3, 0]},
        {index: 3, expectedResultIndices: [1, 2, 0, 3]},
        {index: 2, expectedResultIndices: [1, 0, 2, 3]},
        {index: 1, expectedResultIndices: [0, 1, 2, 3]},
      ],
      expectedEndProfileOrder: ['profile0', 'profile1', 'profile2', 'profile3'],
    });
  });

  test('ProfileReorder_MultipleEntersWithResetOnSameSide', async function() {
    await testProfileReorderingDragCycle({
      expectedInitialProfileOrder:
          ['profile0', 'profile1', 'profile2', 'profile3'],
      dragIndex: 3,
      dragEnterEvents: [
        {index: 1, expectedResultIndices: [0, 3, 1, 2]},
        // Note that entering 1 again here is not the position 1, but the
        // position of the card that has the index 1 (position 2 in the previous
        // expected results).
        {index: 1, expectedResultIndices: [0, 1, 3, 2]},
      ],
      expectedEndProfileOrder: ['profile0', 'profile1', 'profile3', 'profile2'],
    });
  });

  test(
      'ProfileReorder_MultipleEntersOnEachSidesOfTheDraggingCard',
      async function() {
        await testProfileReorderingDragCycle({
          expectedInitialProfileOrder:
              ['profile0', 'profile1', 'profile2', 'profile3'],
          dragIndex: 2,
          dragEnterEvents: [
            {index: 3, expectedResultIndices: [0, 1, 3, 2]},
            {index: 0, expectedResultIndices: [2, 0, 1, 3]},
          ],
          expectedEndProfileOrder:
              ['profile2', 'profile0', 'profile1', 'profile3'],
        });
      });

  test('ProfileReorder_SingleCycle', async function() {
    await testProfileReorderingDragCycles({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragEventCycles: [
        {
          dragIndex: 1,
          dragEnterEvents: [
            {index: 2, expectedResultIndices: [0, 2, 1]},
            {index: 0, expectedResultIndices: [1, 0, 2]},
          ],
          expectedEndProfileOrder: ['profile1', 'profile0', 'profile2'],
        },
      ],
    });
  });

  test('ProfileReorder_MultipleCycles', async function() {
    await testProfileReorderingDragCycles({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragEventCycles: [
        {
          dragIndex: 1,
          dragEnterEvents: [
            {index: 2, expectedResultIndices: [0, 2, 1]},
          ],
          expectedEndProfileOrder: ['profile0', 'profile2', 'profile1'],
        },
        {
          dragIndex: 0,
          dragEnterEvents: [
            {index: 2, expectedResultIndices: [1, 2, 0]},
          ],
          expectedEndProfileOrder: ['profile2', 'profile1', 'profile0'],
        },
      ],
    });
  });

  test('ProfileReorder_TwoIdenticalCyclesAreSymetric', async function() {
    await testProfileReorderingDragCycles({
      expectedInitialProfileOrder: ['profile0', 'profile1', 'profile2'],
      dragEventCycles: [
        {
          dragIndex: 1,
          dragEnterEvents: [
            {index: 2, expectedResultIndices: [0, 2, 1]},
          ],
          expectedEndProfileOrder: ['profile0', 'profile2', 'profile1'],
        },
        {
          dragIndex: 1,
          dragEnterEvents: [
            {index: 2, expectedResultIndices: [0, 2, 1]},
          ],
          expectedEndProfileOrder: ['profile0', 'profile1', 'profile2'],
        },
      ],
    });
  });

  test('ProfileReorder_MultipleCyclesWithMultipleEnters', async function() {
    await testProfileReorderingDragCycles({
      expectedInitialProfileOrder:
          ['profile0', 'profile1', 'profile2', 'profile3'],
      dragEventCycles: [
        {
          dragIndex: 0,
          dragEnterEvents: [
            {index: 1, expectedResultIndices: [1, 0, 2, 3]},
            {index: 2, expectedResultIndices: [1, 2, 0, 3]},
          ],
          expectedEndProfileOrder:
              ['profile1', 'profile2', 'profile0', 'profile3'],
        },
        {
          dragIndex: 3,
          dragEnterEvents: [
            {index: 1, expectedResultIndices: [0, 3, 1, 2]},
            {index: 0, expectedResultIndices: [3, 0, 1, 2]},
            {index: 1, expectedResultIndices: [0, 1, 3, 2]},
          ],
          expectedEndProfileOrder:
              ['profile1', 'profile2', 'profile3', 'profile0'],
        },
        {
          dragIndex: 0,
          dragEnterEvents: [
            {index: 1, expectedResultIndices: [1, 0, 2, 3]},
            {index: 2, expectedResultIndices: [1, 2, 0, 3]},
          ],
          expectedEndProfileOrder:
              ['profile2', 'profile3', 'profile1', 'profile0'],
        },
      ],
    });
  });
});
